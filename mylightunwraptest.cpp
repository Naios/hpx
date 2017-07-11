
#include <hpx/util/pack_traversal.hpp>

using namespace hpx::util;
using namespace hpx::util::detail;

template <typename C, typename... T>
void apply_flatten_impl(std::false_type, C&& callable, T&&... args)
{
    // TODO Check whether any of the args is a detail::flatted_tuple_t,
    //      if not, use the linear called version for better compilation speed.
}

template<typename... T>
using is_any_flatten_t = any_of<

>;

template <typename C, typename... T>
auto apply_flatten(C&& callable, T&&... args)
{
    // Check whether any of the args is a detail::flatted_tuple_t,
    // if not, use the linear called version for better compilation speed.
    return apply_flatten_impl(std::forward<C>(callable), std::forward<T>(args)...);
}

int main(int, char**)
{
    map_pack([](int) -> float { return 0.f; }, 0, 0);

    return 0;
}
