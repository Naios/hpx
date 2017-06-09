//  Copyright (c) 2017      Denis Blank
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <algorithm>
#include <type_traits>

#include <hpx/config.hpp>
#include <hpx/traits/is_range.hpp>
#include <hpx/traits/is_tuple_like.hpp>
#include <hpx/util/always_void.hpp>
#include <hpx/util/invoke_fused.hpp>
#include <hpx/util/tuple.hpp>

// Testing
#include <vector>

// Maybe not needed

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
namespace hpx {
namespace util {
    namespace detail {
        /// Provides utilities for remapping the whole content of a
        /// container like type to the same container holding different types.
        namespace container_remapping {
            /// Deduces to a true type if the given parameter T
            /// is fillable by a std::back_inserter.
            template <typename T, typename = void>
            struct is_back_insertable : std::false_type
            {
            };
            template <typename T>
            struct is_back_insertable<T,
                typename always_void<decltype(std::back_inserter(
                    std::declval<T>()))>::type> : std::true_type
            {
            };

            /// Deduces to a true type if the given parameter T
            /// supports a `reserve` method.
            template <typename T, typename = void>
            struct is_reservable : std::false_type
            {
            };
            template <typename T>
            struct is_reservable<T,
                typename always_void<decltype(std::declval<T>().reserve(
                    std::declval<std::size_t>()))>::type> : std::true_type
            {
            };

            template <typename Dest, typename Source>
            void reserve_if_possible(Dest&, Source&)
            {
                // TODO
            }

            /// Remaps the content of the given container with type T,
            /// to a container of the same type which may contain
            /// different types.
            template <typename M, typename T>
            auto remap(M&& mapper, T&& container)
            {
                static_assert(
                    is_back_insertable<typename std::decay<T>::type>::value,
                    "Can only remap container which are backward insertable!");
            }
        }    // end namespace container_remapping

        /// Tag for dispatching based on the sequenceable or container requirements
        template <bool IsContainer, bool IsSequenceable>
        struct container_match_tag
        {
        };

        // clang-format off
template <typename T>
using container_match_of = container_match_tag<
    hpx::traits::is_range<T>::value,
    hpx::traits::is_tuple_like<T>::value
>;
        // clang-format on

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
            auto traverse(T&& element)    // TODO C++11 auto return
            {
                // TODO Check statically, whether we should traverse the element

                // We use tag dispatching here, to categorize the type T whether
                // it satisfies the container or tuple like requirements.
                // Then we can choose the underlying implementation accordingly.
                using matcher_tag =
                    container_match_of<typename std::decay<T>::type>;
                return match(matcher_tag{}, std::forward<T>(element));
            }

            /// Calls the traversal method for every element in the pack
            template <typename... T>
            auto traverse_pack(/*Remapper remapper,*/ T&&... pack) -> decltype(
                hpx::util::make_tuple(traverse(std::forward<T>(pack))...))
            {
                // TODO Check statically, whether we should traverse the whole pack
                //
                // TODO Use a remapper instead of fixed hpx::util::make_tuple
                return hpx::util::make_tuple(
                    traverse(std::forward<T>(pack))...);
            }

        private:
            struct traversor
            {
                mapping_helper* helper;

                template <typename T>
                auto operator()(T&& element)
                {
                    return helper->traverse(std::forward<T>(element));
                }
            };

            /// Match plain elments
            template <typename T>
            auto match(container_match_tag<false, false>, T&& element)
                -> decltype(mapper_(std::forward<T>(element)))
            {
                // T could be any non container or tuple like type here,
                // take int or hpx::future<int> as an example.
                return mapper_(std::forward<T>(element));
            }

            /// Match elements satisfying the container requirements,
            /// which are not sequenced.
            template <typename T>
            auto match(container_match_tag<true, false>, T&& element)
            {
                // TODO Remap to the specific container type
                using container = std::vector<decltype(*element.begin())>;

                // T could be any container like type here,
                // take std::vector<hpx::future<int>> as an example.
                container remapped;
                helper::reserve_if_possible(
                    remapped, element);    // No forwarding!

                std::transform(element.begin(),
                    element.end(),
                    std::back_inserter(remapped),
                    traversor{this});

                return remapped;
            }

            /// Match elements which are sequenced and that are also may
            /// satisfying the container requirements.
            template <bool IsContainer, typename T>
            auto match(container_match_tag<IsContainer, true>, T&& element)
            {
                // T could be any sequenceable type here,
                // take std::tuple<int, hpx::future<int>> as an example.
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

    }    // end namespace detail
}    // end namespace util
}    // end namespace hpx

using namespace hpx;
using namespace util;
using namespace detail;

void testTraversal()
{
    traverse_pack([](auto&& el) { return el; }, 0, 1, 2);
}

int main(int argc, char* argv[])
{
    std::vector<int> i;
    auto al = i.get_allocator();

    std::true_type tt1 =
        container_remapping::is_back_insertable<std::vector<int>>{};
    std::false_type tt2 = container_remapping::is_back_insertable<int>{};

    testTraversal();
    return 0;
}
