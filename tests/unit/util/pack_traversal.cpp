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
#include <memory>
#include <vector>

using namespace hpx;
using namespace hpx::util;
using namespace hpx::util::detail;

struct all_map_float
{
    template <typename T>
    float operator()(T el) const
    {
        return float(el + 1.f);
    }
};

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

static void testMixedTraversal()
{
    {
        auto res = map_pack(all_map_float{},
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

        static_assert(std::is_same<decltype(res), decltype(expected)>::value,
            "Type mismatch!");
        HPX_TEST((res == expected));
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

static void testMixedEarlyUnwrapped()
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
struct my_allocator : std::allocator<T>
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

static void testMixedContainerRemap()
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
    float operator()(T el) const
    {
        return float(el + 1.f);
    }
};

static void testMixedFallThrough()
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
        [](int /*i*/) {
            // ...
            return 0;
        },
        1, std::vector<int>{2, 3});
}

class counter_mapper
{
    std::reference_wrapper<int> counter_;

public:
    explicit counter_mapper(int& counter)
      : counter_(counter)
    {
    }

    template <typename T>
    void operator()(T el) const
    {
        ++counter_.get();
    }
};

struct test_tag_1
{
};
struct test_tag_2
{
};
struct test_tag_3
{
};

class counter_mapper_rejecting_non_tag_1
{
    std::reference_wrapper<int> counter_;

public:
    explicit counter_mapper_rejecting_non_tag_1(int& counter)
      : counter_(counter)
    {
    }

    void operator()(test_tag_1)
    {
        ++counter_.get();
    }
};

class counter_mapper_rejecting_non_tag_1_sfinae
{
    std::reference_wrapper<int> counter_;

public:
    explicit counter_mapper_rejecting_non_tag_1_sfinae(int& counter)
      : counter_(counter)
    {
    }

    template <typename T,
        typename std::enable_if<std::is_same<typename std::decay<T>::type,
            test_tag_1>>::type* = nullptr>
    void operator()(T)
    {
        ++counter_.get();
    }
};

static void testStrategicTraverse()
{
    // Every element in the pack is visited
    {
        int counter = 0;
        counter_mapper_rejecting_non_tag_1 mapper(counter);
        traverse_pack(mapper, test_tag_1{}, test_tag_2{}, test_tag_3{});
        HPX_TEST_EQ(counter, 3);
    }

    // Every element in the pack is visited from left to right
    {
        int counter = 0;
        traverse_pack(
            [&](int el) {
                HPX_TEST_EQ(counter, el);
                ++counter;
            },
            0, 1, 2, 3);
        HPX_TEST_EQ(counter, 4);
    }

    // Elements accepted by the mapper aren't traversed
    {
        // Signature
        int counter = 0;
        counter_mapper_rejecting_non_tag_1 mapper1(counter);
        traverse_pack(mapper1, test_tag_1{}, test_tag_2{}, test_tag_3{});
        HPX_TEST_EQ(counter, 1);

        // SFINAE
        counter = 0;
        counter_mapper_rejecting_non_tag_1_sfinae mapper2(counter);
        traverse_pack(mapper2, test_tag_1{}, test_tag_2{}, test_tag_3{});
        HPX_TEST_EQ(counter, 1);
    }

    // Remapping works across values
    {}

    // Remapping works across types
    {}

    // Remapping works with move-only objects
    {}

    // Single object remapping returns the value
    {}

    // Remapping multiple objects returns the tuple of objects
    {
    }
}

static void testStrategicContainerTraverse()
{
    // Every element in the container is visited
    {}

    // The container type itself is changed
    {}

    // Every element in the container is remapped
    {
    }
}

static void testStrategicTupleLikeTraverse()
{
    // Every element in the tuple like type is visited
    {}

    // The container tuple like type itself is changed
    {}

    // Every element in the tuple like type is remapped
    {
    }
}

int main(int, char**)
{
    testMixedTraversal();
    testMixedEarlyUnwrapped();
    testMixedContainerRemap();
    testMixedFallThrough();

    testStrategicTraverse();
    testStrategicContainerTraverse();
    testStrategicTupleLikeTraverse();

    return hpx::util::report_errors();
}
