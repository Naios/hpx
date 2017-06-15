//  Copyright (c) 2017 Denis Blank
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/lcos/future.hpp>
#include <hpx/traits/future_traits.hpp>
#include <hpx/traits/is_future.hpp>
#include <hpx/util/lightweight_test.hpp>
#include <hpx/util/pack_traversal.hpp>
#include <algorithm>
#include <vector>

using namespace hpx;
using namespace hpx::util;
using namespace hpx::util::detail;

struct my_mapper
{
    template <typename T,
        typename std::enable_if<std::is_same<T, int>::value>::type* = nullptr>
    float operator()(T el) const
    {
        return float(el + 1.f);
    }
};

struct all_map
{
    template <typename T>
    int operator()(T el) const
    {
        return 0;
    }
};

static void testTraversal()
{
    {
        auto res = map_pack([](auto el) -> float { return float(el + 1.f); },
            0,
            1.f,
            hpx::util::make_tuple(1.f, 3),
            std::vector<std::vector<int>>{{1, 2}, {4, 5}},
            std::vector<std::vector<float>>{{1.f, 2.f}, {4.f, 5.f}},
            2);

        auto expected = hpx::util::make_tuple(    // ...
            1.f,
            2.f,
            hpx::util::make_tuple(2.f, 4.f),
            std::vector<std::vector<float>>{{2.f, 3.f}, {5.f, 6.f}},
            std::vector<std::vector<float>>{{2.f, 3.f}, {5.f, 6.f}},
            3.f);

        //static_assert(std::is_same<decltype(res), decltype(expected)>::value,
        //    "Type mismatch!");
        int i = 0;
        // HPX_TEST((res == expected));
    }

    {
        // Broken build regression tests:
        traverse_pack(my_mapper{}, int(0), 1.f);

        map_pack(all_map{}, 0, std::vector<int>{1, 2});
    }

    {
        // Also a regression test
        auto res = map_pack(all_map{}, std::vector<std::vector<int>>{{1, 2}});
        HPX_TEST_EQ((res[0][0]), (0));
    }

    {
        auto res = map_pack(my_mapper{},
            0,
            1.f,
            hpx::util::make_tuple(1.f, 3,
                std::vector<std::vector<int>>{{1, 2}, {4, 5}},
                std::vector<std::vector<float>>{{1.f, 2.f}, {4.f, 5.f}}),
            2);

        auto expected = hpx::util::make_tuple(    // ...
            1.f,
            1.f,
            hpx::util::make_tuple(1.f, 4.f,
                std::vector<std::vector<float>>{{2.f, 3.f}, {5.f, 6.f}},
                std::vector<std::vector<float>>{{1.f, 2.f}, {4.f, 5.f}}),
            3.f);

        static_assert(std::is_same<decltype(res), decltype(expected)>::value,
            "Type mismatch!");
        HPX_TEST((res == expected));
    }

    {
        int count = 0;
        traverse_pack(
            [&](int el) {
                HPX_TEST_EQ((el), (count + 1));
                count = el;
            },
            1,
            hpx::util::make_tuple(
                2, 3, std::vector<std::vector<int>>{{4, 5}, {6, 7}}));

        HPX_TEST_EQ((count), (7));
    }

    return;
}

struct my_unwrapper
{
    template <typename T,
        typename std::enable_if<traits::is_future<T>::value>::type* = nullptr>
    auto operator()(T future) const ->
        typename traits::future_traits<T>::result_type
    {
        return future.get();
    }
};

static void testEarlyUnwrapped()
{
    using namespace hpx::lcos;

    {
        auto res = map_pack(my_unwrapper{},    // ...
            0, 1, make_ready_future(3),
            make_tuple(make_ready_future(4), make_ready_future(5)));

        auto expected =
            hpx::util::make_tuple(0, 1, 3, hpx::util::make_tuple(4, 5));

        static_assert(std::is_same<decltype(res), decltype(expected)>::value,
            "Type mismatch!");
        HPX_TEST((res == expected));
    }
}

template <typename T>
struct my_allocator : public std::allocator<T>
{
    unsigned state_;

    explicit my_allocator(unsigned state)
      : state_(state)
    {
        return;
    }

    template <typename O>
    my_allocator(my_allocator<O> const& other)
      : state_(other.state_)
    {
        return;
    }
};

static void testContainerRemap()
{
    // Traits
    {
        using namespace container_remapping;
        HPX_TEST_EQ((has_push_back<std::vector<int>, int>::value), true);
        HPX_TEST_EQ((has_push_back<int, int>::value), false);
    }

    // Rebind
    {
        auto const remapper = [](unsigned short i) -> unsigned long {
            return i - 1;
        };

        // Rebinds the values
        {
            std::vector<unsigned short> source = {1, 2, 3};
            std::vector<unsigned long> dest = map_pack(remapper, source);

            HPX_TEST((dest == decltype(dest){0, 1, 2}));
        }

        // Rebinds the allocator
        {
            static unsigned const canary = 7878787;

            my_allocator<unsigned short> allocator(canary);
            std::vector<unsigned short, my_allocator<unsigned short>> source(
                allocator);
            source.push_back(1);

            // TODO Fix this test

            // std::vector<unsigned long, my_allocator<unsigned long>> remapped =
            // remap(strategy_remap_tag{}, source, remapper);

            // HPX_TEST_EQ((remapped.get_allocator().state_), (canary));
        }
    }
}

struct mytester
{
    using traversor_type = mytester;

    int operator()(int)
    {
        return 0;
    }
};

struct my_int_mapper
{
    template <typename T,
        typename std::enable_if<std::is_same<T, int>::value>::type* = nullptr>
    auto operator()(T el) const
    {
        return float(el + 1.f);
    }
};

static void testFallThrough()
{
    traverse_pack(my_int_mapper{}, int(0),
        std::vector<hpx::util::tuple<float, float>>{
            hpx::util::make_tuple(1.f, 2.f)},
        hpx::util::make_tuple(std::vector<float>{1.f, 2.f}));

    traverse_pack(my_int_mapper{}, int(0),
        std::vector<std::vector<float>>{{1.f, 2.f}},
        hpx::util::make_tuple(1.f, 2.f));

    auto res1 = map_pack(my_int_mapper{}, int(0),
        std::vector<std::vector<float>>{{1.f, 2.f}},
        hpx::util::make_tuple(77.f, 2));

    auto res2 = map_pack(
        [](int i) {
            // ...
            return 0;
        },
        1, std::vector<int>{2, 3});

    // hpx::util::make_tuple(strategy_traverse_tag{}),

    int i = 0;
}

int main(int argc, char* argv[])
{
    testTraversal();
    testEarlyUnwrapped();
    testContainerRemap();
    testFallThrough();

    auto result = hpx::util::report_errors();

    return result;
}
