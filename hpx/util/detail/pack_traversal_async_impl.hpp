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
        /// Relocates the given pack with the given offset
        template <std::size_t Offset, typename Pack>
        struct relocate_index_pack;
        template <std::size_t Offset, std::size_t... Sequence>
        struct relocate_index_pack<Offset, pack_c<std::size_t, Sequence...>>
          : std::common_type<pack_c<std::size_t, (Sequence + Offset)...>>
        {
        };

        /// Creates a sequence from begin to end explicitly
        template <std::size_t Begin, std::size_t End>
        using explicit_range_sequence_of_t = typename relocate_index_pack<Begin,
            typename make_index_pack<End - Begin>::type>::type;

        /// Stores the visitor and the arguments to traverse
        template <typename Visitor, typename... Args>
        class async_traversal_frame
          : public std::enable_shared_from_this<
                async_traversal_frame<Visitor, Args...>>
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

            /// Calls the visitor with the given element and a continuation
            /// which is capable of continuing the asynchrone traversal
            /// when it's called later.
            template <typename T, typename Hierarchy>
            void async_continue(T&& value, Hierarchy&& hierarchy)
            {
                // TODO
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

            template <std::size_t Position>
            HPX_CONSTEXPR static_async_range<Target, Position, End> relocate()
                const noexcept
            {
                return static_async_range<Target, Position, End>{target_};
            }
        };
        /// Specialization for the end marker which doesn't provide
        /// a particular element dereference
        template <typename Target, std::size_t Begin>
        struct static_async_range<Target, Begin, Begin>
        {
            explicit static_async_range(Target*)
            {
            }
        };

        template <typename Frame, typename Matcher, typename Current,
            typename... Hierarchy>
        void async_traverse_one_impl(
            Matcher, Frame&& frame, Current&& current, Hierarchy&&... hierarchy)
        {
            // Do nothing if the visitor does't accept the type
        }

        /// \throws async_traversal_detached_exception If the execution context
        ///                                            was detached.
        template <typename Frame, typename Current, typename... Hierarchy>
        auto async_traverse_one_impl(container_match_tag<false, false>,
            Frame&& frame, Current&& current, Hierarchy&&... hierarchy)
            /// SFINAE this out, if the visitor doesn't accept the given element
            -> typename always_void<decltype(frame->traverse(*current))>::type
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

        template <bool IsTupleLike, typename Frame, typename Current,
            typename... Hierarchy>
        void async_traverse_one_impl(container_match_tag<false, IsTupleLike>,
            Frame&& frame, Current&& current, Hierarchy&&... hierarchy)
        {
        }

        template <typename Frame, typename Current, typename... Hierarchy>
        void async_traverse_one_impl(container_match_tag<true, false>,
            Frame&& frame, Current&& current, Hierarchy&&... hierarchy)
        {
        }

        template <typename Frame, typename Current, typename... Hierarchy>
        void async_traverse_one(
            Frame&& frame, Current&& current, Hierarchy&&... hierarchy)
        {
            using ElementType = typename std::decay<decltype(*current)>::type;
            return async_match_impl(container_match_of<ElementType>{},
                std::forward<Frame>(frame), std::forward<Current>(current),
                std::forward<Hierarchy>(hierarchy));
        }

        template <typename Frame, std::size_t... Sequence, typename Current,
            typename Hierarchy>
        void async_traverse_static_async_range(Frame& frame,
            pack_c<std::size_t, Sequence...>, Current&& current,
            Hierarchy&&... hierarchy)
        {
            int dummy[] = {0,
                ((void) async_traverse_one(
                     current.template relocate<Sequence>(), hierarchy...),
                    0)...};
            (void) dummy;
        }

        template <typename Frame, typename Target, std::size_t Begin,
            std::size_t End, typename... Hierarchy>
        void async_traverse(Frame&& frame,
            static_async_range<Target, Begin, End>
                current,
            Hierarchy&&... hierarchy)
        {
            async_traverse_static_async_range(std::forward<Frame>(frame),
                explicit_range_sequence_of_t<Begin, End>{},
                current,
                std::forward<Hierarchy>(hierarchy)...);
        }

        /// Reenter an asynchronous iterator pack and continue its traversal.
        template <typename Current, typename Next, typename... Hierarchy>
        bool resume_traversal(
            Current& current, Next& next, Hierarchy&... hierarchy)
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
