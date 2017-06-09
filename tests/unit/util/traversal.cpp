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
#include <hpx/util/result_of.hpp>
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
            void reserve_if_possible(
                std::true_type, Dest& dest, Source const& source)
            {
                // Reserve the mapped size
                dest.reserve(source.size());
            }

            template <typename Dest, typename Source>
            void reserve_if_possible(
                std::false_type, Dest const&, Source const&)
            {
                // We don't do anything here, since the container doesn't
                // support reserve
            }

            /// Specialization for a container with a single type T
            template <typename NewType,
                template <class> class Base,
                typename OldType>
            auto rebind_container(Base<OldType> const& container)
                -> Base<NewType>
            {
                return Base<NewType>();
            }

            /// Specialization for a container with a single type T and
            /// a particular non templated allocator,
            /// which is preserved across the remap.
            template <typename NewType,
                template <class, class> class Base,
                typename OldType,
                typename Allocator,
                // Check whether the second argument of the container was
                // the used allocator.
                typename std::enable_if<std::is_same<Allocator,
                    decltype(std::declval<Base<OldType, Allocator>>()
                                 .get_allocator())>::value>::type* = nullptr>
            auto rebind_container(Base<OldType, Allocator> const& container)
                -> Base<NewType, Allocator>
            {
                return Base<NewType, Allocator>(container.get_allocator());
            }

            /// Specialization for a container with a single type T and
            /// a particular templated allocator,
            /// which is preserved across the remap.
            /// -> This is the specialization that is used for most
            ///    standard containers.
            template <typename NewType,
                template <class, class> class Base,
                typename OldType,
                template <class> class Allocator,
                // Check whether the second argument of the container was
                // the used allocator.
                typename std::enable_if<std::is_same<Allocator<OldType>,
                    decltype(std::declval<Base<OldType, Allocator<OldType>>>()
                                 .get_allocator())>::value>::type* = nullptr>
            auto rebind_container(
                Base<OldType, Allocator<OldType>> const& container)
                -> Base<NewType, Allocator<NewType>>
            {
                return Base<NewType, Allocator<NewType>>(
                    container.get_allocator());
            }

            /// Returns the type which is resulting if the mapping is applied to
            /// an element in the container.
            template <typename Container, typename Mapping>
            using mapped_type_from =
                decltype(std::declval<typename std::decay<Mapping>::type>()(
                    *std::declval<typename std::decay<Container>::type&>()
                         .begin()));

            /// Remaps the content of the given container with type T,
            /// to a container of the same type which may contain
            /// different types.
            template <typename T, typename M>
            auto remap(T&& container, M&& mapper)
                -> decltype(rebind_container<mapped_type_from<T, M>>(container))
            {
                static_assert(
                    is_back_insertable<typename std::decay<T>::type>::value,
                    "Can only remap containers which are backward insertable!");

                // Create the new container, which is capable of holding
                // the remappped types.
                using mapped = mapped_type_from<T, M>;
                auto remapped = rebind_container<mapped>(container);

                // We try to reserve the original size from the source
                // container to the destination container.
                reserve_if_possible(
                    is_reservable<decltype(remapped)>{}, remapped, container);

                // Perform the actual value remapping from the source to
                // the destination.
                std::transform(std::forward<T>(container).begin(),
                    std::forward<T>(container).end(),
                    std::back_inserter(remapped),
                    std::forward<M>(mapper));

                return remapped;    // RVO
            }
        }    // end namespace container_remapping

        /// Tag for dispatching based on the sequenceable or container requirements
        template <bool IsContainer, bool IsSequenceable>
        struct container_match_tag
        {
        };

        template <typename T>
        using container_match_of =
            container_match_tag<hpx::traits::is_range<T>::value,
                hpx::traits::is_tuple_like<T>::value>;

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

#include <hpx/util/lightweight_test.hpp>
#include <vector>

using namespace hpx;
using namespace util;
using namespace detail;

static void testTraversal()
{
    traverse_pack([](auto&& el) { return el; }, 0, 1, 2);
}

static void testContainerRemap()
{
    using namespace container_remapping;

    // Traits
    HPX_TEST_EQ((is_back_insertable<std::vector<int>>::value), true);
    HPX_TEST_EQ((is_back_insertable<int>::value), false);

    // Rebind
    {
        std::vector<unsigned short> source = {1, 2, 3};
        std::vector<unsigned long> dest =
            remap(source, [](unsigned short i) -> unsigned long { return i; });
    }
}

int main(int argc, char* argv[])
{
    std::vector<int> i;
    auto al = i.get_allocator();

    testTraversal();
    testContainerRemap();

    auto result = hpx::util::report_errors();

    return result;
}
