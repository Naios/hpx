//  Copyright (c) 2017 Denis Blank
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/lcos/future.hpp>
#include <hpx/util/lightweight_test.hpp>
#include <hpx/util/tuple.hpp>
#include <hpx/util/unwrap.hpp>

#include <type_traits>

using namespace hpx::util;
using namespace hpx::lcos;

static void testFutureUnwrap()
{
    {
        int res = unwrap(make_ready_future(0xDD));
        HPX_TEST_EQ((res), (0xDD));
    }

    {
        tuple<int, int> res = unwrap(make_ready_future(make_tuple(0xDD, 0xDF)));
        HPX_TEST((res == make_tuple(0xDD, 0xDF)));
    }

    {
        tuple<int, int> res =
            unwrap(make_ready_future(0xDD), make_ready_future(0xDF));
        HPX_TEST((res == make_tuple(0xDD, 0xDF)));
    }
}

int increment(int c)
{
    return c + 1;
}

static void testFunctionalFutureUnwrap()
{
    // One argument is passed without a tuple unwrap
    {
        auto unwrapper = unwrapping([](int a) { return a; });

        int res = unwrapper(make_ready_future(3));

        HPX_TEST_EQ((res), (3));
    }

    /// Unpack single tuples which were passed to the functional unwrap
    {
        auto unwrapper = unwrapping([](int a, int b) { return a + b; });

        int res = unwrapper(make_ready_future(make_tuple(1, 2)));

        HPX_TEST_EQ((res), (3));
    }

    // Multiple arguments are spread across the callable
    {
        auto unwrapper = unwrapping([](int a, int b) { return a + b; });

        int res = unwrapper(make_ready_future(1), make_ready_future(2));

        HPX_TEST_EQ((res), (3));
    }

    // Regression: (originally taken from the unwrapped tests)
    {
        future<int> future = hpx::make_ready_future(42);
        HPX_TEST_EQ(unwrapping(&increment)(future), 42 + 1);
    }
}

static void testLegacyUnwrap()
{
    // Regression
    {
        std::vector<future<int>> f;
        std::vector<int> res = unwrap(f);
    }

    {
        future<void> f = hpx::make_ready_future();
        using Result = decltype(unwrap(f));
        static_assert(std::is_void<Result>::value, "Failed...");
    }

    {
        future<void> f1 = hpx::make_ready_future();
        future<void> f2 = hpx::make_ready_future();
        future<void> f3 = hpx::make_ready_future();
        using Result = decltype(unwrap(f1, f2, f3));
        static_assert(std::is_void<Result>::value, "Failed...");
    }

    {
        future<void> f = hpx::make_ready_future();

        auto callable = unwrapping([](int, int) {
            // ...
        });

        callable(1, f, 2);
    }

    // local_dataflow_executor_v1.cpp
    // Func=hpx::util::detail::functional_unwrap_impl<hpx::util::detail::old_unwrap_config,void (__cdecl *)(void),1>,
    // Futures=hpx::util::tuple<hpx::lcos::future<void>>
    {
        future<void> f = hpx::make_ready_future();

        auto callable = unwrapping([]() {
            // ...
        });

        callable(f);
    }

    // global_spmd_block.cpp
    //    executor_traits.hpp:340:21: error: void function 'call' should not
    //      return a value [-Wreturn-type]
    //                    return hpx::util::unwrapped(results);
    //                    ^      ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    /// executor_traits.hpp:370:41: note: in instantiation of function
    //      template specialization
    //      'hpx::parallel::v3::detail::bulk_execute_helper::call<hpx::parallel::execution::parallel_executor &,
    //      hpx::lcos::detail::spmd_block_helper<bulk_test_action>, boost::integer_range<unsigned long>, unsigned long
    //      &>' requested here
    //            return bulk_execute_helper::call(0, std::forward<Executor>(exec),
    {
    }
}

int main(int, char**)
{
    testFutureUnwrap();
    testFunctionalFutureUnwrap();

    testLegacyUnwrap();

    return report_errors();
}
