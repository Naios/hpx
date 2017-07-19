//  Copyright (c) 2017 Denis Blank
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef HPX_UTIL_DETAIL_UNWRAP_IMPL_HPP
#define HPX_UTIL_DETAIL_UNWRAP_IMPL_HPP

#include <hpx/config.hpp>
#include <hpx/traits/future_traits.hpp>
#include <hpx/traits/is_future.hpp>
#include <hpx/traits/is_tuple_like.hpp>
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
        /// Deduces to a true_type if the given future is instantiated with
        /// a non void type.
        template <typename T>
        using is_non_void_future = std::integral_constant<bool,
            traits::is_future<T>::value &&
                !std::is_void<
                    typename traits::future_traits<T>::result_type>::value>;

        /// Deduces to a true_type if the given future is instantiated with void
        template <typename T>
        using is_void_future = std::integral_constant<bool,
            traits::is_future<T>::value &&
                std::is_void<
                    typename traits::future_traits<T>::result_type>::value>;

        /// A mapper that maps futures to its representing type
        ///
        /// The mapper does unwrap futures nested inside futures until
        /// the particular given depth.
        ///
        /// - Depth >  1 -> Depth remaining
        /// - Depth == 1 -> One depth remaining
        /// - Depth == 0 -> Unlimited depths
        template <std::size_t Depth>
        struct future_unwrap_until_depth
        {
            /// This piece of code can't be refactored out using
            /// inheritance and `using Base::operator()` because this
            /// isn't taken into account when doing SFINAE.
            template <typename T,
                typename std::enable_if<is_void_future<
                    typename std::decay<T>::type>::value>::type* = nullptr>
            auto operator()(T&& future) const -> decltype(spread_this())
            {
                std::forward<T>(future).get();
                return spread_this();
            }

            template <typename T,
                typename std::enable_if<is_non_void_future<
                    typename std::decay<T>::type>::value>::type* = nullptr>
            auto operator()(T&& future) const
                -> decltype(map_pack(future_unwrap_until_depth<Depth - 1>{},
                    std::forward<T>(future).get()))
            {
                return map_pack(future_unwrap_until_depth<Depth - 1>{},
                    std::forward<T>(future).get());
            }
        };
        template <>
        struct future_unwrap_until_depth<1U>
        {
            /// This piece of code can't be refactored out using
            /// inheritance and `using Base::operator()` because this
            /// isn't taken into account when doing SFINAE.
            template <typename T,
                typename std::enable_if<is_void_future<
                    typename std::decay<T>::type>::value>::type* = nullptr>
            auto operator()(T&& future) const -> decltype(spread_this())
            {
                std::forward<T>(future).get();
                return spread_this();
            }

            template <typename T,
                typename std::enable_if<is_non_void_future<
                    typename std::decay<T>::type>::value>::type* = nullptr>
            auto operator()(T&& future) const -> typename traits::future_traits<
                typename std::decay<T>::type>::result_type
            {
                return std::forward<T>(future).get();
            }
        };
        template <>
        struct future_unwrap_until_depth<0U>
        {
            /// This piece of code can't be refactored out using
            /// inheritance and `using Base::operator()` because this
            /// isn't taken into account when doing SFINAE.
            template <typename T,
                typename std::enable_if<is_void_future<
                    typename std::decay<T>::type>::value>::type* = nullptr>
            auto operator()(T&& future) const -> decltype(spread_this())
            {
                std::forward<T>(future).get();
                return spread_this();
            }

            template <typename T,
                typename std::enable_if<is_non_void_future<
                    typename std::decay<T>::type>::value>::type* = nullptr>
            auto operator()(T&& future) const -> decltype(
                map_pack(std::declval<future_unwrap_until_depth const&>(),
                    std::forward<T>(future).get()))
            {
                return map_pack(*this, std::forward<T>(future).get());
            }
        };

        /// Unwraps the futures contained in the given pack args
        /// until the depth Depth.
        template <std::size_t Depth, typename... Args>
        auto unwrap_depth_impl(Args&&... args)
            -> decltype(map_pack(future_unwrap_until_depth<Depth>{},
                std::forward<Args>(args)...))
        {
            return map_pack(future_unwrap_until_depth<Depth>{},
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

        /// map_pack may return a tuple or a plain type, choose the
        /// corresponding invocation function accordingly.
        template <typename C, typename T>
        auto invoke_wrapped(C&& callable, T&& unwrapped) -> decltype(
            invoke_wrapped_impl<
                traits::is_tuple_like<typename std::decay<T>::type>::value>::
                apply(std::forward<C>(callable), std::forward<T>(unwrapped)))
        {
            return invoke_wrapped_impl<
                traits::is_tuple_like<typename std::decay<T>::type>::value>::
                apply(std::forward<C>(callable), std::forward<T>(unwrapped));
        }

        /// Implements the callable object which is returned by n invocation
        /// to hpx::util::unwrap and similar functions.
        template <typename T, std::size_t Depth>
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
                -> decltype(invoke_wrapped(std::declval<T&>(),
                    unwrap_depth_impl<Depth>(std::forward<Args>(args)...)))
            {
                return invoke_wrapped(wrapped_,
                    unwrap_depth_impl<Depth>(std::forward<Args>(args)...));
            }

            template <typename... Args>
            auto operator()(Args&&... args) const
                -> decltype(invoke_wrapped(std::declval<T const&>(),
                    unwrap_depth_impl<Depth>(std::forward<Args>(args)...)))
            {
                return invoke_wrapped(wrapped_,
                    unwrap_depth_impl<Depth>(std::forward<Args>(args)...));
            }
        };

        /// Returns a callable object which unwraps the futures
        /// contained in the given pack args until the depth Depth.
        template <std::size_t Depth, typename T>
        auto functional_unwrap_depth_impl(T&& callable)
            -> functional_unwrap_impl<typename std::decay<T>::type, Depth>
        {
            return functional_unwrap_impl<typename std::decay<T>::type, Depth>(
                std::forward<T>(callable));
        }
    }
}    // end namespace util
}    // end namespace hpx

#endif    // HPX_UTIL_DETAIL_UNWRAP_IMPL_HPP
