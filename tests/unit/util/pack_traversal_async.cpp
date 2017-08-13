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
    std::reference_wrapper<std::size_t> counter_;

public:
    explicit async_increasing_int_sync_visitor(
        std::reference_wrapper<std::size_t>
            counter)
      : counter_(counter)
    {
    }

    bool operator()(std::size_t i) const
    {
        HPX_TEST_EQ(i, counter_.get());
        ++counter_.get();
        return true;
    }

    template <typename N>
    void operator()(std::size_t i, N&& next)
    {
        HPX_UNUSED(i);
        HPX_UNUSED(next);

        // Should never be called!
        HPX_TEST(false);
    }

    void operator()() const
    {
        HPX_TEST_EQ(counter_.get(), ArgCount);
        ++counter_.get();
    }
};

template <std::size_t ArgCount>
class async_increasing_int_visitor
{
    std::reference_wrapper<std::size_t> counter_;

public:
    explicit async_increasing_int_visitor(
        std::reference_wrapper<std::size_t> counter)
      : counter_(counter)
    {
    }

    bool operator()(std::size_t i) const
    {
        HPX_TEST_EQ(i, counter_.get());
        return false;
    }

    template <typename N>
    void operator()(std::size_t i, N&& next)
    {
        HPX_UNUSED(i);

        ++counter_.get();
        std::forward<N>(next)();
    }

    void operator()() const
    {
        HPX_TEST_EQ(counter_.get(), ArgCount);
        ++counter_.get();
    }
};

template <std::size_t ArgCount>
class async_increasing_int_interrupted_visitor
{
    std::reference_wrapper<std::size_t> counter_;

public:
    explicit async_increasing_int_interrupted_visitor(
        std::reference_wrapper<std::size_t>
            counter)
      : counter_(counter)
    {
    }

    bool operator()(std::size_t i) const
    {
        HPX_TEST_EQ(i, counter_.get());
        ++counter_.get();

        // Detach the control flow at the second step
        return i == 0;
    }

    template <typename N>
    void operator()(std::size_t i, N&& next)
    {
        HPX_TEST_EQ(i, 1U);
        HPX_TEST_EQ(counter_.get(), 2U);

        // Don't call next here
        HPX_UNUSED(next);
    }

    void operator()() const
    {
        // Will never be called
        HPX_TEST(false);
    }
};

template <std::size_t ArgCount, typename... Args>
void test_async_traversal_base(Args&&... args)
{
    // Test that every element is traversed in the correct order
    // when we detach the control flow on every visit.
    {
        std::size_t counter = 0U;
        traverse_pack_async(
            async_increasing_int_sync_visitor<ArgCount>(std::ref(counter)),
            args...);
        HPX_TEST_EQ(counter, ArgCount + 1U);
    }

    // Test that every element is traversed in the correct order
    // when we detach the control flow on every visit.
    {
        std::size_t counter = 0U;
        traverse_pack_async(
            async_increasing_int_visitor<ArgCount>(std::ref(counter)), args...);
        HPX_TEST_EQ(counter, ArgCount + 1U);
    }

    // Test that the first element is traversed only,
    // if we don't call the resume continuation.
    {
        std::size_t counter = 0U;
        traverse_pack_async(async_increasing_int_interrupted_visitor<ArgCount>(
                                std::ref(counter)),
            args...);
        HPX_TEST_EQ(counter, 2U);
    }
}

static void test_async_traversal()
{
    // Just test everything using a casual int pack
    test_async_traversal_base<4U>(0U, 1U, 2U, 3U);
}

static void test_async_tuple_like_traversal()
{
    // Test by passing a tuple in the middle
    test_async_traversal_base<4U>(0U, make_tuple(1U, 2U), 3U);
    // Test by splitting the pack in two tuples
    test_async_traversal_base<4U>(make_tuple(0U, 1U), make_tuple(2U, 3U));
    // Test by passing a huge tuple to the traversal
    test_async_traversal_base<4U>(make_tuple(0U, 1U, 2U, 3U));
}

int main(int, char**)
{
    test_async_traversal();
    test_async_tuple_like_traversal();

    {
        std::size_t counter = 0U;
        traverse_pack_async(
            async_increasing_int_interrupted_visitor<4>(std::ref(counter)), 0U,
            make_tuple(1U, 2U), 3U);
        HPX_TEST_EQ(counter, 2U);
    }

    return hpx::util::report_errors();
}
