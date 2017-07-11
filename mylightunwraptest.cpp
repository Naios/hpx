
#include <hpx/util/pack_traversal.hpp>

using namespace hpx::util;
using namespace hpx::util::detail;

/// A callable object which returns its input wrapped inside a tuple
struct tupelize
{
    template <typename... T>
    tuple<T...> operator()(T&&... args) const
    {
        return tuple<T...>{std::forward<T>(args)...};
    }
};

/// Use the linear instantiation for non flat types
template <typename C, typename... T>
auto apply_flatten_impl(std::false_type, C&& callable, T&&... args)
{
    // TODO bla
    return 0;
}

/// Use the recursive instantiation for a variadic pack which may contain
/// flat types
template <typename C, typename... T>
void apply_flatten_impl(std::true_type, C&& callable, T&&... args)
{
}

template <typename... T>
using is_any_flatten_t = any_of<is_flat<T>...>;

template <typename C, typename... T>
auto apply_flatten(C&& callable, T&&... args)
    -> decltype(apply_flatten_impl(is_any_flatten_t<T...>{},
        std::forward<C>(callable), std::forward<T>(args)...))
{
    // Check whether any of the args is a detail::flatted_tuple_t,
    // if not, use the linear called version for better compilation speed.
    return apply_flatten_impl(is_any_flatten_t<T...>{},
        std::forward<C>(callable), std::forward<T>(args)...);
}

int main(int, char**)
{
    map_pack([](int) -> float { return 0.f; }, 0, 0);

    return 0;
}
