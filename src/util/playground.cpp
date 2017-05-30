//  Copyright (c) 2017      Denis Blank
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/config.hpp>
#include <cstddef>
#include <exception>

// Testing
#include <string>

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
            /// A type which doesn't belong to any other category above
            struct plain_type_tag { };
        } // namespace categories

        template<typename T>
        using category_of_t = void;

        // ...
    } // namespace detail

    template <typename... T>
    void immediate_unwrap(T&&... /*arguments*/)
    {
    }

    namespace detail {

    } // end namespace detail

    template <typename T>
    void full_unwrappable(T&& /*callable*/)
    {
        /// ...
    }

    template <std::size_t N, typename T>
    void n_unwrappable(T&& /*callable*/)
    {
        /// ...
    }

    template <typename T>
    void unwrappable(T&& /*callable*/)
    {
        /// ...
    }
} } // end namespace hpx::util

using namespace hpx::util;

struct drop_by_default { };

template<typename T>
struct mocked_future
{
    mocked_future() { }

    template<typename O>
    mocked_future(mocked_future<O>) { }

    template<typename C = drop_by_default>
    mocked_future unwrap(C /*error_handler*/ = drop_by_default{})
    {
        return {};
    }
};

template<typename T>
T mocked_unwrap(T t)
{
    return t;
}

template<typename T>
mocked_future<void> mocked_dataflow(T...)
{
    return {};
}

// Unwraps all futures by default
template<typename T>
mocked_future<void> mocked_plain_dataflow(T...)
{
    return {};
}

mocked_future<std::string> http_request(std::string /*url*/)
{
    return {};
}

void testNewUnwrapped()
{
        // Without exception handling
    mocked_future<void> f1 = mocked_dataflow([](std::string /*content*/)
    {
        // ...
    }, http_request("github.com"));

    // Without exception handling
    mocked_future<void> f2 = mocked_dataflow([](std::string /*content*/)
    {
        // ...
    }, http_request("github.com").unwrap());

    // Seperated exception handler
    mocked_future<void> f3 = mocked_dataflow([](std::string /*content*/)
    {
        // ...
    }, http_request("github.com").unwrap([](std::exception_ptr /*exception*/)
    {
        // ...
    }));

    // Unwraps all futures by default, forwards exceptions to its returning
    // future.
    mocked_future<unsigned> f4 =
        mocked_plain_dataflow([](std::string content)
    {
        // ...
        return content.size();
    }, http_request("github.com"));
}
