//  Copyright (c) 2017 Denis Blank
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/config.hpp>
#include <hpx/lcos/future.hpp>
#include <hpx/util/pack_traversal_async.hpp>
#include <hpx/util/tuple.hpp>
#include <hpx/util/unused.hpp>
#include "hpx/util/lightweight_test.hpp"

#include <cstdint>
#include <functional>
#include <type_traits>
#include <utility>

using hpx::lcos::future;
using hpx::util::make_tuple;
using hpx::util::traverse_pack_async;
using hpx::util::tuple;

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

template <std::size_t ArgCount>
class async_increasing_int_sync_visitor
{
    std::reference_wrapper<int> counter_;

public:
    explicit async_increasing_int_sync_visitor(std::reference_wrapper<int>
            counter)
      : counter_(counter)
    {
    }

    bool operator()(int i) const
    {
        HPX_TEST_EQ(i, counter_.get());
        ++counter_.get();
        return true;
    }

    template <typename N>
    void operator()(int i, N&& next)
    {
        HPX_UNUSED(i);
        HPX_UNUSED(next);

        // Should never be called!
        HPX_TEST(false);
    }

    void operator()() const
    {
        HPX_TEST_EQ(counter_.get(), ArgCount);
    }
};

template <std::size_t ArgCount>
class async_increasing_int_visitor
{
    std::reference_wrapper<int> counter_;

public:
    explicit async_increasing_int_visitor(std::reference_wrapper<int> counter)
      : counter_(counter)
    {
    }

    bool operator()(int i) const
    {
        HPX_TEST_EQ(i, counter_.get());
        return false;
    }

    template <typename N>
    void operator()(int i, N&& next)
    {
        HPX_UNUSED(i);

        ++counter_.get();
        std::forward<N>(next)();
    }

    void operator()() const
    {
        HPX_TEST_EQ(counter_.get(), ArgCount);
    }
};

template <std::size_t ArgCount>
class async_increasing_int_interrupted_visitor
{
    std::reference_wrapper<int> counter_;

public:
    explicit async_increasing_int_interrupted_visitor(
        std::reference_wrapper<int>
            counter)
      : counter_(counter)
    {
    }

    bool operator()(int i) const
    {
        HPX_TEST_EQ(i, counter_.get());
        ++counter_.get();

        // Detach the control flow at the second step
        return i == 0;
    }

    template <typename N>
    void operator()(int i, N&& next)
    {
        HPX_TEST_EQ(i, 1);
        HPX_TEST_EQ(counter_.get(), 2);

        // Don't call next here
        HPX_UNUSED(next);
    }

    void operator()() const
    {
        // Will never be called
        HPX_TEST(false);
    }
};

template <typename... Args>
void test_async_traversal_base(Args&&... args)
{
    // Test that every element is traversed in the correct order
    // when we detach the control flow on every visit.
    {
        int counter = 0;
        traverse_pack_async(async_increasing_int_sync_visitor<sizeof...(args)>(
                                std::ref(counter)),
            args...);
        HPX_TEST_EQ(counter, sizeof...(args));
    }

    // Test that every element is traversed in the correct order
    // when we detach the control flow on every visit.
    {
        int counter = 0;
        traverse_pack_async(
            async_increasing_int_visitor<sizeof...(args)>(std::ref(counter)),
            args...);
        HPX_TEST_EQ(counter, sizeof...(args));
    }

    // Test that the first element is traversed only,
    // if we don't call the resume continuation.
    {
        int counter = 0;
        traverse_pack_async(
            async_increasing_int_interrupted_visitor<sizeof...(args)>(
                std::ref(counter)),
            args...);
        HPX_TEST_EQ(counter, 2);
    }
}

static void test_async_traversal()
{
    // Just test everything using a casual int pack
    test_async_traversal_base(0, 1, 2, 3);
}

static void test_async_tuple_like_traversal()
{
    // Test by passing a tuple in the middle
    test_async_traversal_base(0, make_tuple(1, 2), 3);
    // Test by splitting the pack in two tuples
    test_async_traversal_base(make_tuple(0, 1), make_tuple(2, 3));
    // Test by passing a huge tuple to the traversal
    test_async_traversal_base(make_tuple(0, 1, 2, 3));
}

int main(int, char**)
{
    // test_async_traversal();
    // test_async_tuple_like_traversal();

    {
        int counter = 0;
        traverse_pack_async(async_increasing_int_sync_visitor<4>(
                                std::ref(counter)),
            make_tuple(0, 1, 2, 3));
        HPX_TEST_EQ(counter, 4);
    }

    return hpx::util::report_errors();
}
