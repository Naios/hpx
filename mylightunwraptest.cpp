
#include <hpx/util/pack_traversal.hpp>

int main(int, char**)
{
    using hpx::util::map_pack;
    using hpx::util::spread_this;
    using namespace hpx::util::detail::spreading;

    hpx::util::tuple<int, int, int, int> res1 =
        tupelize(0, 0, spread_this(0, 0));

    hpx::util::tuple<int, int, int> res2 = tupelize(0, 0, 0);

    map_pack([](int) -> float { return 0.f; }, 0, 0);

    return 0;
}
