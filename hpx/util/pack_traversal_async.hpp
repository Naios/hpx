//  Copyright (c) 2017 Denis Blank
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef HPX_UTIL_PACK_TRAVERSAL_ASYNC_HPP
#define HPX_UTIL_PACK_TRAVERSAL_ASYNC_HPP

#include <hpx/util/detail/pack_traversal_async_impl.hpp>

#include <utility>

namespace hpx {
namespace util {
    /// Traverses the pack with the given visitor in an asynchronous way.
    ///
    /// This function works in the same way as `traverse_pack`,
    /// however, we are able to suspend and continue the traversal at
    /// later time.
    ///
    /// See `traverse_pack` for a detailed description.
    template <typename Visitor, typename... T>
    void traverse_pack_async(Visitor&& visitor, T&&... pack)
    {
    }
}    // end namespace util
}    // end namespace hpx

#endif    // HPX_UTIL_PACK_TRAVERSAL_ASYNC_HPP
