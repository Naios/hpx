//  Copyright (c) 2017      Denis Blank
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/config.hpp>

namespace hpx { namespace util {
    namespace detail {
        namespace categories {
            /// Any hpx::future or hpx::shared_future
            struct future_type_tag { };
            /// Any homogeneous container satisfying the container requirements
            struct container_type_tag { };
            /// Any heterogeneous container type which is accessable through
            /// a call to hpx::util::get().
            struct sequenced_type_tag { };
            /// A type which doesn't belong to any other category below
            struct plain_type_tag { };
        } // namespace categories

        template<typename T>
        using category_of_t = void;

        // ...
    } // namespace detail

    struct hey
    {
    };

    template <typename... T>
    void immediate_unwrap(T&&... /*arguments*/)
    {
    }

    namespace detail {

    } // end namespace detail

    template <typename T>
    void make_unwrappable(T&& /*callable*/)
    {
    }
} // end namespace hpx::util

using namespace hpx::util;

void testNewUnwrapped()
{
}
