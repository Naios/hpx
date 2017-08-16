//  Copyright (c) 2017 Denis Blank
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#if !defined(HPX_TRAITS_IS_FUTURE_ITERATOR_HPP)
#define HPX_TRAITS_IS_FUTURE_ITERATOR_HPP

#include <hpx/traits/is_future.hpp>
#include <hpx/traits/is_iterator.hpp>

#include <type_traits>

namespace hpx {
namespace traits {
    /// Deduces to a true_type if the given type is an iterator over futures
    template <typename Iterator, typename Enable = void>
    struct is_future_iterator : std::false_type
    {
    };

    template <typename Iterator>
    struct is_future_iterator<Iterator,
        typename std::enable_if<is_iterator<Iterator>::value &&
            is_future<decltype(*std::declval<Iterator>())>::value>::type>
      : std::true_type
    {
    };
}    // end namespace traits
}    //end namespace hpx

#endif    // HPX_TRAITS_IS_FUTURE_ITERATOR_HPP
