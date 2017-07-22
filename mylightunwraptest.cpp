
#include <hpx/lcos/future.hpp>
#include <hpx/util/pack_traversal.hpp>
#include <hpx/util/unwrap.hpp>
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

    {
        auto unwrapper = [](auto&& ar) {
            // ...
            return true;
        };

        hpx::util::tuple<std::array<hpx::lcos::future<int>, 10>> in;

        unwrapper(in);
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
