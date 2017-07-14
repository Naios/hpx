
#include <hpx/util/pack_traversal.hpp>
#include <cassert>
#include <vector>

/// A mapper which duplicates the given element
struct duplicate_mapper
{
    template <typename T>
    auto operator()(T arg) -> decltype(hpx::util::spread_this(arg, arg))
    {
        return hpx::util::spread_this(arg, arg);
    }
};

/// A mapper which removes the current element
struct zero_mapper
{
    template <typename T>
    auto operator()(T arg) -> decltype(hpx::util::spread_this())
    {
        return hpx::util::spread_this();
    }
};

static void testSpreadTraverse()
{
    using hpx::util::map_pack;
    using hpx::util::spread_this;
    using hpx::util::tuple;
    using hpx::util::make_tuple;
    using namespace hpx::util::detail::spreading;

    // 1:2 mappings (multiple arguments)
    /*{
        tuple<int, int, int, int> res = map_pack(duplicate_mapper{}, 1, 2);

        tuple<int, int, int, int> expected = make_tuple(1, 1, 2, 2);

        assert((res == expected));
    }

    // 1:0 mappings
    {
        using Result = decltype(map_pack(zero_mapper{}, 1, 1.f));

        /// We expect a void result if all elements were reoved by the mapping
        assert((std::is_void<Result>::value));
    }

    // 1:2 mappings (multiple arguments)
    {
        std::vector<tuple<int, int>> res =
            map_pack(duplicate_mapper{}, std::vector<int>{1});

        std::vector<tuple<int, int>> expected;
        expected.push_back(make_tuple(1, 1));

        assert((res == expected));
    }

    // 1:0 mappings
    {
        std::vector<hpx::util::tuple<>> res =
            map_pack(zero_mapper{}, std::vector<int>{1});
        assert(res.size() == 1U);
    }*/

    // 1:2 mappings (multiple arguments)
    {
        tuple<tuple<int, int, int, int>> res =
            map_pack(duplicate_mapper{}, make_tuple(make_tuple(1, 2)));

        tuple<tuple<int, int, int, int>> expected =
            make_tuple(make_tuple(1, 1, 2, 2));

        assert((res == expected));
    }
}

int main(int, char**)
{
    using hpx::util::map_pack;
    using hpx::util::spread_this;
    using namespace hpx::util::detail::spreading;

    testSpreadTraverse();

    return 0;
}
