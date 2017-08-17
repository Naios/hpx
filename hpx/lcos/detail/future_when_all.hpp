//  Copyright (c) 2017 Denis Blank
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef HPX_LCOS_DETAIL_FUTURE_WHEN_ALL_HPP
#define HPX_LCOS_DETAIL_FUTURE_WHEN_ALL_HPP

#include <hpx/lcos/future.hpp>
#include <hpx/traits/acquire_shared_state.hpp>
#include <hpx/traits/detail/reserve.hpp>
#include <hpx/traits/is_future.hpp>
#include <hpx/traits/is_future_iterator.hpp>
#include <hpx/util/deferred_call.hpp>

#include <type_traits>
#include <utility>
#include <vector>

namespace hpx {
namespace lcos {
    namespace detail {
        /// Returns true when the given future is ready,
        /// the future is deferred executed if possible first.
        template <typename T,
            typename std::enable_if<traits::is_future<
                typename std::decay<T>::type>::value>::type* = nullptr>
        bool async_visit_future(T&& current)
        {
            auto state =
                traits::detail::get_shared_state(std::forward<T>(current));

            if ((state.get() == nullptr) || state->is_ready())
            {
                return true;
            }

            // Execute_deferred might have made the future ready
            state->execute_deferred();

            // Detach the context
            return state->is_ready();
        }

        /// Attach the continuation next to the given future
        template <typename T, typename N,
            typename std::enable_if<traits::is_future<
                typename std::decay<T>::type>::value>::type* = nullptr>
        void async_detach_future(T&& current, N&& next)
        {
            auto state =
                traits::detail::get_shared_state(std::forward<T>(current));

            // Attach a continuation to this future which will
            // re-evaluate it and continue to the next argument
            // (if any).
            state->set_on_completed(util::deferred_call(std::forward<N>(next)));
        }

        /// Helper for disambiguating the two iterator overload from the
        /// overload taking two arguments.
        template <typename Dispatcher, typename First, typename Second>
        auto dispatch_iterator_overload(
            Dispatcher&& dispatcher, First&& first, Second&& second)
            -> decltype(std::forward<Dispatcher>(dispatcher)(
                std::forward<First>(first), std::forward<Second>(second)))
        {
            return std::forward<Dispatcher>(dispatcher)(
                std::forward<First>(first), std::forward<Second>(second));
        }
        template <typename Dispatcher, typename Iterator,
            typename Container =
                std::vector<typename future_iterator_traits<Iterator>::type>>
        future<Container> dispatch_iterator_overload(
            Dispatcher&& dispatcher, Iterator begin, Iterator end)
        {
            Container lazy_values_;

            auto difference = std::distance(begin, end);
            if (difference > 0)
                traits::detail::reserve_if_reservable(
                    lazy_values_, static_cast<std::size_t>(difference));

            std::transform(begin, end, std::back_inserter(lazy_values_),
                traits::acquire_future_disp());

            return std::forward<Dispatcher>(dispatcher)(
                std::move(lazy_values_));
        }
    }    // end namespace detail
}    // end namespace lcos
}    // end namespace hpx

#endif    // HPX_LCOS_DETAIL_FUTURE_WHEN_ALL_HPP
