//  Copyright (c) 2017 Denis Blank
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/lcos/future.hpp>
#include <hpx/util/lightweight_test.hpp>
#include <hpx/util/tuple.hpp>
#include <hpx/util/unwrap.hpp>

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

    /// Pass single tuples shrough which were passed to the functional unwrap
    {
        auto unwrapper =
            unwrapping([](tuple<int, int> a) { return get<0>(a) + get<1>(a); });

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
}

int main(int, char**)
{
    testFutureUnwrap();
    testFunctionalFutureUnwrap();

    testLegacyUnwrap();

    return report_errors();
}
