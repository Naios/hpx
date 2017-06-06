//  Copyright (c) 2017      Denis Blank
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <type_traits>

#include <hpx/config.hpp>
#include <hpx/util/tuple.hpp>

// #include <hpx/traits/is_future.hpp>

// #include <hpx/lcos/future.hpp>
// #include <hpx/traits/future_traits.hpp>
//
// #include <hpx/traits/is_future_range.hpp>
// #include <hpx/traits/is_future_tuple.hpp>
// #include <hpx/util/detail/pack.hpp>
// #include <hpx/util/invoke_fused.hpp>
// #include <hpx/util/lazy_enable_if.hpp>
// #include <hpx/util/result_of.hpp>
//

// #include <hpx/traits/concepts.hpp>
// template <typename F, typename ... Args,
//   HPX_CONCEPT_REQUIRES_(hpx::traits::is_action<F>::value)
// >

/*
struct future_mapper
{
    template <typename T>
    using should_visit = std::true_type;

    template <typename T>
    T operator()(mocked::future<T>& f) const
    {
        // Do something on f
        return f.get();
    }
};

template <typename... T> void unused(T&&... args) {
  auto use = [](auto&& type) mutable {
    (void)type;
    return 0;
  };
  auto deduce = {0, use(std::forward<decltype(args)>(args))...};
  (void)deduce;
  (void)use;
}
*/

template <typename Mapper, typename T>
auto map_single_element(Mapper&& map, T&& element)
{
}

/// A helper class which applies the mapping or routes the element through
template <typename M>
class mapping_helper
{
    M mapper_;

public:
    explicit mapping_helper(M mapper)
      : mapper_(std::move(mapper))
    {
    }

    /*template <typename T>
    auto operator()(T&& element) const
    {
        return element;
    }*/

    template <typename T>
    auto traverse(T&& element) const
    {
        return element;
    }

    template <typename... T>
    auto traverse_pack(T&&... pack)
        -> decltype(hpx::util::make_tuple(traverse(std::forward<T>(pack))...))
    {
        // TODO check statically whether we should traverse the whole pack
        return hpx::util::make_tuple(traverse(std::forward<T>(pack))...);
    }
};

template <typename Mapper, typename... T>
auto traverse_pack(Mapper&& mapper, T&&... pack)
{
    mapping_helper<typename std::decay<Mapper>::type> helper(
        std::forward<Mapper>(mapper));
    return helper.traverse_pack(std::forward<T>(pack)...);
}

void testTraversal()
{
    traverse_pack([](auto&& el) { return el; }, 0, 1, 2);
}
