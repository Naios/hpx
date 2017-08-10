//  Copyright (c) 2017 Denis Blank
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef HPX_UTIL_DETAIL_PACK_TRAVERSAL_ASYNC_IMPL_HPP
#define HPX_UTIL_DETAIL_PACK_TRAVERSAL_ASYNC_IMPL_HPP

#include <hpx/config.hpp>
#include <hpx/util/always_void.hpp>
#include <hpx/util/detail/pack.hpp>
#include <hpx/util/detail/pack_traversal_impl.hpp>
#include <hpx/util/invoke.hpp>
#include <hpx/util/tuple.hpp>

#include <exception>
#include <functional>    // reference_wrapper
#include <memory>
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

        /// An internally used exception to detach the current execution context
        struct async_traversal_detached_exception : std::exception
        {
            explicit async_traversal_detached_exception()
            {
            }

            char const* what() const override
            {
                return "The execution context was detached!";
            }
        };

        template <typename From, typename To>
        struct async_range
        {
        };

        template <typename Target, std::size_t Begin, std::size_t End>
        struct static_async_range
        {
            Target* target_;

            HPX_CONSTEXPR auto operator*() const noexcept
                -> decltype(util::get<Begin>(target_))
            {
                return util::get<Begin>(target_);
            }

            /*HPX_CONSTEXPR static_async_range<Target, Rest...> shift() const
                noexcept
            {
                return static_async_range<Target, Rest...>{target_};
            }*/
        };

        /// \throws async_traversal_detached_exception If the execution context
        ///                                            was detached.
        template <typename Frame, typename Current, typename... Hierarchy>
        void async_traverse_one(
            Frame&& frame, Current&& current, Hierarchy&&... hierarchy)
        {
            if (!frame->traverse(*current))
            {
                // Store the current call hierarchy into a tuple for
                // later reentrance.
                auto state = util::make_tuple(std::forward<Current>(current),
                    std::forward<Hierarchy>(hierarchy)...);

                // If the traversal method returns false, we detach the
                // current execution context and call the visitor with the
                // element and a continue callable object again.
                frame->async_continue(*current, std::move(state));

                // Then detach the current execution context through throwing
                // an async_traversal_detached_exception which is catched
                // below the traversal call hierarchy.
                throw async_traversal_detached_exception{};
            }
        }

        template <typename Frame, std::size_t... Sequence, typename Range,
            typename Hierarchy>
        bool async_traverse_static_async_range(Frame& frame,
            pack_c<std::size_t, Sequence...>, Range& range,
            Hierarchy&&... hierarchy) noexcept(false)
        {
            bool proceed = false;

            auto invoker = [&](auto&& element) {
                if (proceed)
                {
                    proceed = async_traverse_one(element, hierarchy);
                }
            };

            int dummy[] = {0, ((void) (range.template at<Sequence>()), 0)...};
            (void) dummy;

            return proceed;
        }

        template <typename Frame, typename Target, std::size_t Begin,
            std::size_t End, typename Rest>
        bool async_traverse(Frame& frame,
            static_async_range<Target, Begin, End> range, Rest& rest)
        {
        }

        /// Reenter an asynchronous iterator pack and continue its traversal.
        template <typename Current, typename Next, typename... Hierarchy>
        bool reenter(Current& current, Next& next, Hierarchy&... hierarchy)
        {
            try
            {
                /// TODO Resolve the recursion here
                async_traverse(std::forward<Current>(current),
                    util::forward_as_tuple(std::forward<Next>(next),
                        std::forward<Hierarchy>(hierarchy)...));
                return reenter(std::forward<Next>(next).shift(),
                    std::forward<Hierarchy>(hierarchy)...);
            }
            catch (async_traversal_detached_exception const&)
            {
                return false;
            }
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

            frame->head();
        }
    }    // end namespace detail
}    // end namespace util
}    // end namespace hpx

#endif    // HPX_UTIL_DETAIL_PACK_TRAVERSAL_ASYNC_IMPL_HPP
