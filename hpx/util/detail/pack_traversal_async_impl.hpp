//  Copyright (c) 2017 Denis Blank
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef HPX_UTIL_DETAIL_PACK_TRAVERSAL_ASYNC_IMPL_HPP
#define HPX_UTIL_DETAIL_PACK_TRAVERSAL_ASYNC_IMPL_HPP

#include <hpx/config.hpp>
#include <hpx/util/always_void.hpp>
#include <hpx/util/detail/container_category.hpp>
#include <hpx/util/detail/pack.hpp>
#include <hpx/util/invoke.hpp>
#include <hpx/util/tuple.hpp>

#include <exception>
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
            explicit HPX_CONSTEXPR async_traversal_frame(Visitor visitor,
                Args... args)
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

        /// Represents a particular point in a asynchronous traversal hierarchy
        template <typename Frame, typename Hierarchy>
        class async_traversal_point
        {
            Frame frame_;
            Hierarchy hierarchy_;

        public:
            explicit async_traversal_point(Frame frame, Hierarchy hierarchy)
              : frame_(std::move(frame))
              , hierarchy_(std::move(hierarchy))
            {
            }

            /// Async traverse a single element, and do nothing.
            /// This function is matched last.
            template <typename Matcher, typename Current>
            void async_traverse_one_impl(Matcher, Current&& current)
            {
                // Do nothing if the visitor does't accept the type
            }

            /// Async traverse a single element which isn't a container or
            /// tuple like type. This function is SFINAEd out if the element
            /// isn't accepted by the visitor.
            ///
            /// \throws async_traversal_detached_exception If the execution
            ///         context was detached, an exception is thrown to
            ///         stop the traversal.
            template <typename Current>
            auto async_traverse_one_impl(container_category_tag<false, false>,
                Current&& current)
                /// SFINAE this out, if the visitor doesn't accept
                /// the given element
                -> typename always_void<decltype(
                    std::declval<Frame>()->traverse(*current))>::type
            {
                if (!frame_->traverse(*current))
                {
                    // Store the current call hierarchy into a tuple for
                    // later reentrance.
                    auto state = util::tuple_cat(
                        util::make_tuple(std::forward<Current>(current)),
                        std::move(hierarchy_));

                    // If the traversal method returns false, we detach the
                    // current execution context and call the visitor with the
                    // element and a continue callable object again.
                    frame_->async_continue(*current, std::move(state));

                    // Then detach the current execution context through throwing
                    // an async_traversal_detached_exception which is catched
                    // below the traversal call hierarchy.
                    throw async_traversal_detached_exception{};
                }
            }

            /// Async traverse a single element which is a container or
            /// tuple like type.
            template <bool IsTupleLike, typename Current>
            void async_traverse_one_impl(
                container_category_tag<false, IsTupleLike>,
                Current&& current)
            {
            }

            /// Async traverse a single element which is a tuple like type only.
            template <typename Current>
            void async_traverse_one_impl(container_category_tag<true, false>,
                Current&& current)
            {
            }

            /// Async traverse the current iterator
            template <typename Current>
            void async_traverse_one(Current&& current)
            {
                using ElementType =
                    typename std::decay<decltype(*current)>::type;
                return async_match_impl(container_category_of_t<ElementType>{},
                    std::forward<Current>(current));
            }

            template <std::size_t... Sequence, typename Current>
            void async_traverse_static_async_range(
                pack_c<std::size_t, Sequence...>,
                Current&& current)
            {
                int dummy[] = {0,
                    ((void) async_traverse_one(
                         current.template relocate<Sequence>()),
                        0)...};
                (void) dummy;
            }

            template <typename Target, std::size_t Begin, std::size_t End>
            void async_traverse(static_async_range<Target, Begin, End> current)
            {
                async_traverse_static_async_range(
                    explicit_range_sequence_of_t<Begin, End>{}, current);
            }
        };

        /// Reenter an asynchronous iterator pack and continue its traversal.
        template <typename Current, typename Next, typename... Hierarchy>
        bool resume_traversal(Current& current,
            Next& next,
            Hierarchy&... hierarchy)
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
