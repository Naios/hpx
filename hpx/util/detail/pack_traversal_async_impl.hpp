//  Copyright (c) 2017 Denis Blank
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef HPX_UTIL_DETAIL_PACK_TRAVERSAL_ASYNC_IMPL_HPP
#define HPX_UTIL_DETAIL_PACK_TRAVERSAL_ASYNC_IMPL_HPP

#include <hpx/config.hpp>
#include <hpx/util/always_void.hpp>
#include <hpx/util/detail/pack_traversal_impl.hpp>
#include <hpx/util/invoke.hpp>

#include <functional>    // reference_wrapper
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

namespace hpx {
namespace util {
    namespace detail {
        /// Stores the visitor and the arguments to traverse
        template <typename Visitor, typename... Args>
        class async_traversal_frame
        {
            Visitor visitor_;
            tuple<Args...> args_;

        public:
            explicit HPX_CONSTEXPR async_traversal_frame(
                Visitor visitor, Args... args)
              : visitor_(std::move(visitor))
              , args_(util::make_tuple(std::move(args)...))
            {
            }

            /// Returns the arguments of the frame
            HPX_CONSTEXPR tuple<Args...>& head() noexcept
            {
                return args_;
            }

            /// Calls the visitor with the given element
            template <typename T>
            auto traverse(T&& value)
                -> decltype(util::invoke(visitor_, std::forward<T>(value)))
            {
                return util::invoke(visitor_, std::forward<T>(value));
            }
        };

        template <typename From, typename To>
        struct async_range
        {
        };

        template <typename Target, std::size_t... Sequence>
        struct static_async_range
        {
            Target* target_;

            HPX_CONSTEXPR Target* operator*() const noexcept
            {
                return target_;
            }
        };

        /// Trav
        template <typename Frame, typename Target, std::size_t... Sequence,
            typename... Rest>
        bool async_traverse(Frame& frame,
            static_async_range<Target, Sequence...> range, Rest&... rest)
        {
            bool aborted = false;

            return aborted;
        }

        /// Reenter an asynchronous iterator pack
        template <typename Current, typename Next, typename... Rest>
        bool reenter(Current& current, Next& next, Rest&... rest)
        {
            if (async_traverse(std::forward<Current>(current),
                    std::forward<Next>(next), std::forward<Rest>(current)...))
            {
                return reenter(std::forward<Next>(next).shift(),
                    std::forward<Rest>(current)...);
            }
            return false;
        }

        /// Traverses the given pack with the given mapper
        template <typename Mapper, typename... T>
        void apply_pack_transform_async(Mapper&& mapper, T&&... pack)
        {
            using FrameType =
                async_traversal_frame<typename std::decay<Mapper>::type,
                    typename std::decay<T>::type...>;

            auto frame = std::make_shared<FrameType>(
                std::forward<Mapper>(mapper), std::forward<T>(pack));
        }
    }    // end namespace detail
}    // end namespace util
}    // end namespace hpx

#endif    // HPX_UTIL_DETAIL_PACK_TRAVERSAL_ASYNC_IMPL_HPP
