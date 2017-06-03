//  Copyright (c) 2017      Denis Blank
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/config.hpp>
#include <cstddef>
#include <exception>

// Testing
#include <string>
// #include <hpx/include/threads.hpp>
// #include <hpx/include/local_lcos.hpp>
// #include <hpx/hpx.hpp>
// #include <hpx/hpx_init.hpp>
#include <hpx/lcos/future.hpp>
#include <hpx/lcos/when_all.hpp>
#include <hpx/dataflow.hpp>
#include <hpx/util/unwrapped.hpp>
#include "hpx/include/async.hpp"

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

namespace mocked {
struct drop_by_default
{
};

template <typename T>
struct future
{
    future()
    {
    }

    template <typename O>
    future(future<O>)
    {
    }

    template <typename C = drop_by_default>
    future unwrap(C /*error_handler*/ = drop_by_default{})
    {
        return {};
    }

    T get()
    {
        return {};
    }
};

template <typename T>
T unwrap(T t)
{
    return t;
}

template <typename T>
future<void> dataflow(T...)
{
    return {};
}

// Unwraps all futures by default
template <typename T>
future<void> plain_dataflow(T...)
{
    return {};
}

future<std::string> http_request(std::string /*url*/)
{
    return {};
}
} // end namespace mocked

void testNewUnwrapped()
{
    using namespace mocked;

    // Without exception handling
    future<void> f1 = dataflow(
        [](future<std::string> /*content*/) {
            // ...
        },
        http_request("github.com"));

    // Without exception handling
    future<void> f2 = dataflow(
        [](std::string /*content*/) {
            // ...
        },
        http_request("github.com").unwrap());

    // Seperated exception handler
    future<void> f3 = dataflow(
        [](std::string /*content*/) {
            // ...
        },
        http_request("github.com").unwrap([](std::exception_ptr /*exception*/) {
            // ...
        }));

    // Unwraps all futures by default, forwards exceptions to its returning
    // future.
    future<unsigned> f4 = plain_dataflow(
        [](std::string content) {
            // ...
            return content.size();
        },
        http_request("github.com"));
}

static void future_int_f1(int f1) {  }

void thenVsDataflow()
{
    hpx::future<int> f1, f2, f3; // Dummy vector

    hpx::future<std::size_t> res1 =
        hpx::when_all(f1, f2, f3)
            .then([](hpx::future<hpx::util::tuple<hpx::future<int>,
                          hpx::future<int>, hpx::future<int>>>
                          v) -> std::size_t {
                //
                return 0;
            });

    auto fn = static_cast<std::size_t (*)(hpx::future<int>)>(
        [](hpx::future<int>) -> std::size_t {
            //
            return 0;
        });

    auto res2 = hpx::dataflow(hpx::launch::async, fn, hpx::make_ready_future(1));

    hpx::future<int> ff = hpx::async([] { return 0; });

    auto res4 = hpx::dataflow(&future_int_f1, 0);

    auto res3 = hpx::dataflow([] (int) { }, 1);

    hpx::make_ready_future(0)
        .then([](hpx::future<int> i) { return 0; })
        .then([](hpx::future<int> i) {
            
        });
}
