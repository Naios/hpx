
#include <hpx/util/pack_traversal.hpp>

namespace hpx {
namespace util {
    namespace detail {
        namespace spreading {
            /// Use the recursive instantiation for a variadic pack which
            /// may contain spread types
            template <typename C, typename... T>
            auto apply_spread_impl(std::true_type, C&& callable, T&&... args)
                -> decltype(invoke_fused(std::forward<C>(callable),
                    tuple_cat(undecorate(std::forward<T>(args))...)))
            {
                return invoke_fused(std::forward<C>(callable),
                    tuple_cat(undecorate(std::forward<T>(args))...));
            }

            /// Use the linear instantiation for variadic packs which don't
            /// contain spread types.
            template <typename C, typename... T>
            auto apply_spread_impl(std::false_type, C&& callable, T&&... args)
                -> typename invoke_result<C, T...>::type
            {
                return invoke(
                    std::forward<C>(callable), std::forward<T>(args)...);
            }

            /// Deduces to a true_type if any of the given types marks
            /// the underlying type to be spread into the current context.
            template <typename... T>
            using is_any_spread_t = any_of<is_spread<T>...>;

            template <typename C, typename... T>
            auto apply_spread(C&& callable, T&&... args)
                -> decltype(apply_spread_impl(is_any_spread_t<T...>{},
                    std::forward<C>(callable), std::forward<T>(args)...))
            {
                // Check whether any of the args is a detail::flatted_tuple_t,
                // if not, use the linear called version for better
                // compilation speed.
                // TODO Maybe short circuit this
                return apply_spread_impl(is_any_spread_t<T...>{},
                    std::forward<C>(callable), std::forward<T>(args)...);
            }

            /// A callable object which returns its input wrapped inside a tuple
            struct functional_tupelize
            {
                template <typename... T>
                tuple<T...> operator()(T&&... args) const
                {
                    return tuple<T...>{std::forward<T>(args)...};
                }
            };

            template <typename... T>
            auto tupelize(T&&... args) -> decltype(
                apply_spread(functional_tupelize{}, std::forward<T>(args)...))
            {
                return apply_spread(
                    functional_tupelize{}, std::forward<T>(args)...);
            }
        }
    }
}
}

int main(int, char**)
{
    using hpx::util::map_pack;
    using hpx::util::spread_this;
    using namespace hpx::util::detail::spreading;

    tupelize(0, 0, spread_this(0, 0));

    map_pack([](int) -> float { return 0.f; }, 0, 0);

    return 0;
}
