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
            return true;
        });

        HPX_TEST((callable(f)));
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

    // ... and ...

    //1>sequential_executor_v1.cpp
    //1>e:\projekte\hpx\hpx\parallel\executors\sequenced_executor.hpp(127): error C2562: "hpx::parallel::execution::sequenced_executor::bulk_sync_execute": "void"-Funktion gibt einen Wert zurück
    //1>e:\projekte\hpx\hpx\parallel\executors\sequenced_executor.hpp(123): note: Siehe Deklaration von "hpx::parallel::execution::sequenced_executor::bulk_sync_execute"
    //1>e:\projekte\hpx\hpx\parallel\executors\sequenced_executor.hpp(210): note: Siehe Verweis auf die Instanziierung der gerade kompilierten Funktions-Vorlage "hpx::parallel::execution::detail::bulk_execute_result_impl<F,Shape,true,_Ty>::type hpx::parallel::execution::sequenced_executor::bulk_sync_execute<hpx::util::detail::bound<void (__cdecl *(const hpx::util::detail::placeholder<1> &,hpx::thread::id &,const hpx::util::detail::placeholder<2> &))(int,hpx::thread::id,int)>,S,_Ty>(F &&,const S &,_Ty &&)".
    //1>        with
    //1>        [
    //1>            F=hpx::util::detail::bound<void (__cdecl *(const hpx::util::detail::placeholder<1> &,hpx::thread::id &,const hpx::util::detail::placeholder<2> &))(int,hpx::thread::id,int)>,
    //1>            Shape=std::vector<int,std::allocator<int>>,
    //1>            _Ty=int,
    //1>            S=std::vector<int,std::allocator<int>>
    //1>        ]
    {
        /*
        TODO: Fix this
        std::vector<hpx::lcos::future<void>> vec;
        (void) vec;
        using Result = decltype(unwrap(vec));
        static_assert(std::is_void<Result>::value, "Failed...");*/
    }

    /*1>local_dataflow_std_array.cpp
    1>e:\projekte\hpx\hpx\lcos\dataflow.hpp(129): error C2039: "type": Ist kein Element von "hpx::lcos::detail::dataflow_return<Func,Futures,void>"
    1>        with
    1>        [
    1>            Func=hpx::util::detail::functional_unwrap_impl<int (__cdecl *)(const std::array<int,10> &),1>,
    1>            Futures=hpx::util::tuple<std::array<hpx::lcos::future<int>,10>>
    1>        ]
    1>e:\projekte\hpx\hpx\lcos\dataflow.hpp(129): note: Siehe Deklaration von "hpx::lcos::detail::dataflow_return<Func,Futures,void>"
    1>        with
    1>        [
    1>            Func=hpx::util::detail::functional_unwrap_impl<int (__cdecl *)(const std::array<int,10> &),1>,
    1>            Futures=hpx::util::tuple<std::array<hpx::lcos::future<int>,10>>
    1>        ]
    1>e:\projekte\hpx\hpx\lcos\local\dataflow.hpp(239): note: Siehe Verweis auf die Klasse Vorlage-Instanziierung "hpx::lcos::detail::dataflow_frame<hpx::launch,hpx::util::detail::functional_unwrap_impl<int (__cdecl *)(const std::array<int,10> &),1>,hpx::util::tuple<Range>>", die kompiliert wird.*/
    //
    // Func=hpx::util::detail::functional_unwrap_impl<int (__cdecl *)(const std::array<int,10> &),1>,
    // Futures=hpx::util::tuple<std::array<hpx::lcos::future<int>,10>>
    {
        auto unwrapper = unwrapping([](std::array<int, 2> const& ar) {
            // ...
            return true;
        });

        hpx::util::tuple<std::array<hpx::lcos::future<int>, 2>> in =
            hpx::util::make_tuple(std::array<hpx::lcos::future<int>, 2>{
                {hpx::lcos::make_ready_future(1),
                    hpx::lcos::make_ready_future(2)}});

        HPX_TEST((unwrapper(in)));
    }

    {
        std::array<hpx::lcos::future<int>, 2> in{
            {hpx::lcos::make_ready_future(1), hpx::lcos::make_ready_future(2)}};

        std::array<int, 2> result = unwrap(in);

        HPX_TEST((result == std::array<int, 2>{{1, 2}}));
    }

    {
        auto unwrapper = unwrapping([](auto&&... ar) {
            // ...
            return true;
        });

        std::array<hpx::lcos::future<int>, 2> in{
            {hpx::lcos::make_ready_future(1), hpx::lcos::make_ready_future(2)}};

        auto result = unwrapper(in);
        HPX_TEST((result));
    }
}

int main(int, char**)
{
    testFutureUnwrap();
    testFunctionalFutureUnwrap();

    testLegacyUnwrap();

    return report_errors();
}
