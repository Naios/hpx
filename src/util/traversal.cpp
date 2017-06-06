//  Copyright (c) 2017      Denis Blank
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <type_traits>

#include <hpx/config.hpp>
#include <hpx/util/tuple.hpp>
#include <hpx/traits/is_range.hpp>

#include <hpx/util/invoke_fused.hpp>

// #include <hpx/traits/is_future.hpp>

// #include <hpx/lcos/future.hpp>
// #include <hpx/traits/future_traits.hpp>
//
//
//
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

/// Tag for dispatching based on the sequenceable or container requirements
template <bool IsContainer, bool IsSequenceable>
struct container_match_tag
{
};

template <typename T>
using container_match_of = container_match_tag<hpx::traits::is_range<T>::value,

    >;

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

    /// Traverses a single element
    template <typename T>
    auto traverse(T&& element) const -> T
    {
        // TODO Check statically, whether we should traverse the element
        return element;
    }

    /// Calls the traversal method for every element in the pack
    template <typename... T>
    auto traverse_pack(/*Remapper remapper,*/ T&&... pack)
        -> decltype(hpx::util::make_tuple(traverse(std::forward<T>(pack))...))
    {
        // TODO Check statically, whether we should traverse the whole pack
        //
        // TODO Use a remapper instead of fixed hpx::util::make_tuple
        return hpx::util::make_tuple(traverse(std::forward<T>(pack))...);
    }

private:
    /// Match plain elments
    template <typename T>
    auto match(container_match_tag<false, false>, T&& element)
        -> decltype(mapper_(std::forward<T>(element)))
    {
        return mapper_(std::forward<T>(element));
    }

    /// Match elements satisfying the container requirements,
    /// which are not sequenced.
    template <typename T>
    auto match(container_match_tag<true, false>, T&& element)
    {
        // TODO
    }

    /// Match elements which are sequenced and that are also may
    /// satisfying the container requirements.
    template <bool IsContainer, typename T>
    auto match(container_match_tag<IsContainer, true>, T&& element)
    {
        // TODO
    }
};

/// Traverses the given pack with the given mapper
template <typename Mapper, typename... T>
auto traverse_pack(Mapper&& mapper, T&&... pack) -> decltype(
    std::declval<mapping_helper<typename std::decay<Mapper>::type>>()
        .traverse_pack(std::forward<T>(pack)...))
{
    mapping_helper<typename std::decay<Mapper>::type> helper(
        std::forward<Mapper>(mapper));
    return helper.traverse_pack(std::forward<T>(pack)...);
}

void testTraversal()
{
    traverse_pack([](auto&& el) { return el; }, 0, 1, 2);
}
