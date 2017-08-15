//  Copyright (c) 2017 Denis Blank
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef HPX_LCOS_DETAIL_FUTURE_WHEN_ALL_HPP
#define HPX_LCOS_DETAIL_FUTURE_WHEN_ALL_HPP

#include <hpx/traits/acquire_shared_state.hpp>
#include <hpx/traits/is_future.hpp>
#include <hpx/util/deferred_call.hpp>

#include <type_traits>
#include <utility>

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

    }    // end namespace detail
}    // end namespace lcos
}    // end namespace hpx

#endif    // HPX_LCOS_DETAIL_FUTURE_WHEN_ALL_HPP
