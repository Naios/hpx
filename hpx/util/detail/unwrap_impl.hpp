//  Copyright (c) 2017 Denis Blank
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef HPX_UTIL_DETAIL_UNWRAP_IMPL_HPP
#define HPX_UTIL_DETAIL_UNWRAP_IMPL_HPP

#include <hpx/config.hpp>
#include <hpx/lcos/future.hpp>
#include <hpx/traits/future_traits.hpp>
#include <hpx/traits/is_future.hpp>
#include <hpx/traits/is_tuple_like.hpp>
#include <hpx/util/identity.hpp>
#include <hpx/util/invoke.hpp>
#include <hpx/util/invoke_fused.hpp>
#include <hpx/util/pack_traversal.hpp>
#include <hpx/util/result_of.hpp>

#include <cstddef>
#include <type_traits>
#include <utility>

namespace hpx {
namespace util {
    namespace detail {
        /// This struct defines a configurateable environment in order
        /// to adapt the unwrapping behaviour.
        ///
        /// The old unwrap (unwrapped) allowed futures to be instanted
        /// with void whereas the new one doesn't.
        ///
        /// The old unwrap (unwrapped) also unwraps the first occuring future
        /// independently a single value or multiple ones were passed to it.
        /// This behaviour is inconsistent and was removed in the
        /// new implementation.
        template <bool AllowVoidFutures, bool UnwrapTopLevelTuples>
        struct unwrap_config
        {
        };

        /// The old unwrapped implementation
        using old_unwrap_config = unwrap_config<true, true>;
        /// The new unwrap implementation
        using new_unwrap_config = unwrap_config<false, false>;

        /// A tag which may replace void results when unwrapping
        struct unwrapped_void_tag
        {
        };

        template <typename Child, typename Config>
        struct unwrap_base;

        /// A mapper that maps futures to its representing type
        ///
        /// The mapper does unwrap futures nested inside futures until
        /// the particular given depth.
        ///
        /// - Depth >  1 -> Depth remaining
        /// - Depth == 1 -> One depth remaining
        /// - Depth == 0 -> Unlimited depths
        template <std::size_t Depth, typename Config>
        struct future_unwrap_until_depth
          : unwrap_base<future_unwrap_until_depth<Depth, Config>, Config>
        {
            using unwrap_base<future_unwrap_until_depth<Depth, Config>,
                Config>::operator();

            template <typename T>
            auto transform(T&& future) const -> decltype(
                map_pack(future_unwrap_until_depth<Depth - 1, Config>{},
                    std::forward<T>(future).get()))
            {
                return map_pack(future_unwrap_until_depth<Depth - 1, Config>{},
                    std::forward<T>(future).get());
            }
        };
        template <typename Config>
        struct future_unwrap_until_depth<1U, Config>
          : unwrap_base<future_unwrap_until_depth<1U, Config>, Config>
        {
            using unwrap_base<future_unwrap_until_depth<1U, Config>, Config>::
            operator();

            template <typename T>
            auto transform(T&& future) const -> typename traits::future_traits<
                typename std::decay<T>::type>::result_type
            {
                return std::forward<T>(future).get();
            }
        };
        template <typename Config>
        struct future_unwrap_until_depth<0U, Config>
          : unwrap_base<future_unwrap_until_depth<0U, Config>, Config>
        {
            using unwrap_base<future_unwrap_until_depth<0U, Config>, Config>::
            operator();

            /// The mapper unwraps futures that are nested inside other futures
            /// until an arbitrary depth.
            /// Thus the return value will contain no future.
            template <typename T>
            auto transform(T&& future) const -> decltype(
                map_pack(std::declval<future_unwrap_until_depth const&>(),
                    std::forward<T>(future).get()))
            {
                return map_pack(*this, std::forward<T>(future).get());
            }
        };

        template <typename Child, bool AllowVoidFutures,
            bool UnwrapTopLevelTuples>
        struct unwrap_base<Child,
            unwrap_config<AllowVoidFutures, UnwrapTopLevelTuples>>
        {
            template <typename T>
            unwrapped_void_tag evaluate(util::identity<void>, T&& future) const
            {
                static_assert(AllowVoidFutures && std::is_same<T, T>::value,
                    "Unwrapping future<void> or shared_future<void> is "
                    "forbidden! Use hpx::lcos::wait_all instead!");
                std::forward<T>(future).get();
                return {};
            }

            template <typename R, typename T>
            auto evaluate(util::identity<R>, T&& future) const
                -> decltype(std::declval<Child const*>()->transform(
                    std::forward<T>(future)))
            {
                return static_cast<Child const*>(this)->transform(
                    std::forward<T>(future));
            }

            template <typename T,
                typename std::enable_if<traits::is_future<
                    typename std::decay<T>::type>::value>::type* = nullptr>
            auto operator()(T&& future) const
                -> decltype(std::declval<unwrap_base>().evaluate(
                    util::identity<typename traits::future_traits<
                        typename std::decay<T>::type>::result_type>{},
                    std::forward<T>(future)))
            {
                return evaluate(
                    util::identity<typename traits::future_traits<
                        typename std::decay<T>::type>::result_type>{},
                    std::forward<T>(future));
            }
        };

        /// Unwraps the futures contained in the given pack args
        /// until the depth Depth.
        template <std::size_t Depth, typename Config, typename... Args>
        auto unwrap_depth_impl(Config, Args&&... args)
            -> decltype(map_pack(future_unwrap_until_depth<Depth, Config>{},
                std::forward<Args>(args)...))
        {
            return map_pack(future_unwrap_until_depth<Depth, Config>{},
                std::forward<Args>(args)...);
        }

        /// We use a specialized class here because MSVC has issues with
        /// tag dispatching a function because it does semantical checks before
        /// matching the tag, which leads to false errors.
        template <bool IsFusedInvoke>
        struct invoke_wrapped_impl
        {
            /// Invoke the callable with the tuple argument through invoke_fused
            template <typename C, typename T>
            static auto apply(C&& callable, T&& unwrapped)
                // There is no trait for the invoke_fused result
                -> decltype(invoke_fused(std::forward<C>(callable),
                    std::forward<T>(unwrapped)))
            {
                return invoke_fused(
                    std::forward<C>(callable), std::forward<T>(unwrapped));
            }
        };
        template <>
        struct invoke_wrapped_impl<false /*IsFusedInvoke*/>
        {
            /// Invoke the callable with the plain argument through invoke,
            /// also when the result is a tuple like type, when we received
            /// a single argument.
            template <typename C, typename T>
            static auto apply(C&& callable, T&& unwrapped) ->
                typename invoke_result<C, T>::type
            {
                return util::invoke(
                    std::forward<C>(callable), std::forward<T>(unwrapped));
            }
        };

        /// Indicates whether we should invoke the result through invoke_fused:
        /// - We called the function with multiple arguments.
        /// - The result is a tuple like type and UnwrapTopLevelTuples is set.
        template <bool HadMultipleArguments, bool UnwrapTopLevelTuples,
            typename Result>
        using should_fuse_invoke = std::integral_constant<bool,
            HadMultipleArguments ||
                (UnwrapTopLevelTuples &&
                    traits::is_tuple_like<
                        typename std::decay<Result>::type>::value)>;

        /// map_pack may return a tuple or a plain type, choose the
        /// corresponding invocation function accordingly.
        template <bool HadMultipleArguments, bool AllowVoidFutures,
            bool UnwrapTopLevelTuples, typename C, typename T>
        auto invoke_wrapped(
            unwrap_config<AllowVoidFutures, UnwrapTopLevelTuples>, C&& callable,
            T&& unwrapped)
            -> decltype(invoke_wrapped_impl<
                should_fuse_invoke<HadMultipleArguments, UnwrapTopLevelTuples,
                    T>::value>::apply(std::forward<C>(callable),
                std::forward<T>(unwrapped)))
        {
            return invoke_wrapped_impl<
                should_fuse_invoke<HadMultipleArguments, UnwrapTopLevelTuples,
                    T>::value>::apply(std::forward<C>(callable),
                std::forward<T>(unwrapped));
        }

        /// Implements the callable object which is returned by n invocation
        /// to hpx::util::unwrap and similar functions.
        template <typename Config, typename T, std::size_t Depth>
        class functional_unwrap_impl
        {
            /// The wrapped callable object
            T wrapped_;

        public:
            explicit functional_unwrap_impl(T wrapped)
              : wrapped_(std::move(wrapped))
            {
            }

            template <typename... Args>
            auto operator()(Args&&... args)
                -> decltype(invoke_wrapped<(sizeof...(args) > 1)>(Config{},
                    std::declval<T&>(),
                    unwrap_depth_impl<Depth>(Config{},
                        std::forward<Args>(args)...)))
            {
                return invoke_wrapped<(sizeof...(args) > 1)>(Config{}, wrapped_,
                    unwrap_depth_impl<Depth>(
                        Config{}, std::forward<Args>(args)...));
            }

            template <typename... Args>
            auto operator()(Args&&... args) const
                -> decltype(invoke_wrapped(Config{}, std::declval<T const&>(),
                    unwrap_depth_impl<Depth>(Config{},
                        std::forward<Args>(args)...)))
            {
                return invoke_wrapped(Config{}, wrapped_,
                    unwrap_depth_impl<Depth>(
                        Config{}, std::forward<Args>(args)...));
            }
        };

        /// Returns a callable object which unwraps the futures
        /// contained in the given pack args until the depth Depth.
        template <std::size_t Depth, typename Config, typename T>
        auto functional_unwrap_depth_impl(Config, T&& callable)
            -> functional_unwrap_impl<Config, typename std::decay<T>::type,
                Depth>
        {
            return functional_unwrap_impl<Config, typename std::decay<T>::type,
                Depth>(std::forward<T>(callable));
        }
    }
}    // end namespace util
}    // end namespace hpx

#endif    // HPX_UTIL_DETAIL_UNWRAP_IMPL_HPP
