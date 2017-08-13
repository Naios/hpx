//  Copyright (c) 2013 Hartmut Kaiser
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define HPX_NO_VERSION_CHECK

#include <hpx/config.hpp>
#include <hpx/util/lightweight_test.hpp>
#include <iostream>

#if defined(HPX_WINDOWS) && defined(HPX_MSVC)
#include <WinBase.h>
#endif    // defined(HPX_WINDOWS) && defined(HPX_MSVC)

namespace hpx { namespace util { namespace detail
{
    void test_failed_debugger_break()
    {
#if !defined(NDEBUG)
#if defined(HPX_WINDOWS) && defined(HPX_MSVC)
        if (IsDebuggerPresent())
        {
            // Break because a unit test failed
            DebugBreak();
        }
#endif    // defined(HPX_WINDOWS) && defined(HPX_MSVC)
#endif    // !defined(NDEBUG)
   }

    fixture global_fixture{std::cerr};
}}}

