
#include <hpx/util/pack_traversal.hpp>
#include <cassert>

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
    {
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
}

int main(int, char**)
{
    using hpx::util::map_pack;
    using hpx::util::spread_this;
    using namespace hpx::util::detail::spreading;

    testSpreadTraverse();

    /*hpx::util::tuple<int, int, int, int> res1 =
        tupelize(0, 0, spread_this(0, 0));

    hpx::util::tuple<int, int, int> res2 = tupelize(0, 0, 0);

    map_pack([](int) -> float { return 0.f; }, 0, 0);*/

    return 0;
}
