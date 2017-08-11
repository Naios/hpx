//  Copyright (c) 2017 Denis Blank
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/lcos/future.hpp>
#include <hpx/util/pack_traversal_async.hpp>
#include "hpx/util/lightweight_test.hpp"

using hpx::lcos::future;
using hpx::util::traverse_pack_async;

struct async_int_visitor
{
    bool operator()(int& i) const
    {
        return false;
    }

    template <typename N>
    void operator()(int& i, N&& next) const
    {
        i = 0;
        std::forward<N>(next)();
    }

    void operator()() const
    {
    }
};

struct async_future_visitor
{
    // Called for every future in the pack, if the method returns false,
    // the asynchronous handler is called again.
    template <typename T>
    bool operator()(future<T>& f) const
    {
        return f.is_ready();    // Call the asynchronous handler again
    }

    template <typename T, typename N>
    void operator()(future<T>& f, N&& next) const
    {
        // C++14 version may be converted to C++11
        f.then([next = std::forward<N>(next)](
            auto&&...) mutable { std::move(next)(); });
    }

    // Called when the end is reached
    void operator()() const
    {
        // promise.resolve(...) ...
    }
};

static void test_async_traversal()
{
    {
        traverse_pack_async(async_int_visitor{}, 1, 2, 3, 4);
    }
}

int main(int, char**)
{
    test_async_traversal();

    return hpx::util::report_errors();
}
