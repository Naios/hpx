//  Copyright (c) 2017 Denis Blank
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <algorithm>
#include <type_traits>

#include <hpx/config.hpp>
#include <hpx/traits/is_range.hpp>
#include <hpx/traits/is_tuple_like.hpp>
#include <hpx/util/always_void.hpp>
#include <hpx/util/invoke.hpp>
#include <hpx/util/invoke_fused.hpp>
#include <hpx/util/lazy_enable_if.hpp>
#include <hpx/util/result_of.hpp>
#include <hpx/util/tuple.hpp>

// Testing
#include <hpx/lcos/future.hpp>
#include <hpx/traits/is_future.hpp>
#include <hpx/util/lightweight_test.hpp>
#include <algorithm>
#include <vector>

namespace hpx {
namespace util {
    namespace detail {
        /// Just traverses the pack with the given callable object,
        /// no result is returned or preserved.
        struct strategy_traverse_tag
        {
        };
        /// Remaps the variadic pack with the return values from the mapper.
        struct strategy_remap_tag
        {
        };

        /// Provides utilities for remapping the whole content of a
        /// container like type to the same container holding different types.
        namespace container_remapping {
            /// Deduces to a true type if the given parameter T
            /// has a push_back method that accepts a type of E.
            template <typename T, typename E, typename = void>
            struct has_push_back : std::false_type
            {
            };
            template <typename T, typename E>
            struct has_push_back<T, E,
                typename always_void<decltype(std::declval<T>().push_back(
                    std::declval<E>()))>::type> : std::true_type
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

            /// Deduces to a true type if the given parameter T
            /// returns an object of type Allocator from `get_allocator()`.
            template <typename T, typename Allocator, typename = void>
            struct is_using_allocator : std::false_type
            {
            };
            template <typename T, typename Allocator>
            struct is_using_allocator<T, Allocator,
                typename always_void<typename std::enable_if<std::is_same<
                    Allocator, decltype(std::declval<T>().get_allocator())>::
                        value>::type>::type> : std::true_type
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
            template <typename NewType, template <class> class Base,
                typename OldType>
            auto rebind_container(Base<OldType> const& container)
                -> Base<NewType>
            {
                return Base<NewType>();
            }

            /// Specialization for a container with a single type T and
            /// a particular non templated allocator,
            /// which is preserved across the remap.
            template <typename NewType, template <class, class> class Base,
                typename OldType, typename Allocator,
                // Check whether the second argument of the container was
                // the used allocator.
                typename std::enable_if<is_using_allocator<
                    Base<OldType, Allocator>, Allocator>::value>::type* =
                    nullptr>
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
            template <typename NewType, template <class, class> class Base,
                typename OldType, template <class> class Allocator,
                // Check whether the second argument of the container was
                // the used allocator.
                typename std::enable_if<
                    is_using_allocator<Base<OldType, Allocator<OldType>>,
                        Allocator<OldType>>::value>::type* = nullptr>
            auto rebind_container(
                Base<OldType, Allocator<OldType>> const& container)
                -> Base<NewType, Allocator<NewType>>
            {
                return Base<NewType, Allocator<NewType>>(
                    container.get_allocator());
            }

            /// Deduces to the type the homogeneous container is containing
            template <typename Container>
            using element_of_t = decltype(*std::declval<Container>().begin());

            /// Returns the type which is resulting if the mapping is applied to
            /// an element in the container.
            template <typename Container, typename Mapping>
            using mapped_type_from_t =
                typename invoke_result<Mapping, element_of_t<Container>>::type;

            /// Remaps the content of the given container with type T,
            /// to a container of the same type which may contain
            /// different types.
            template <typename T, typename M>
            auto remap(strategy_remap_tag, T&& container, M&& mapper)
                -> decltype(
                    rebind_container<mapped_type_from_t<T, M>>(container))
            {
                // TODO Maybe optimize this for the case where we map to the
                // same type, so we don't create a whole new container for
                // that case.

                static_assert(has_push_back<typename std::decay<T>::type,
                                  element_of_t<T>>::value,
                    "Can only remap containers which provide a push_back "
                    "method!");

                // Create the new container, which is capable of holding
                // the remappped types.
                using mapped = mapped_type_from_t<T, M>;
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

            /*template <typename T, typename M>
            struct guard_traverse
            {
                using type = decltype(
                    std::declval<M>().as_traversor()(std::declval<T>()));
            };*/

            /// Just call the visitor with the content of the container
            template <typename T, typename M>
            auto remap(strategy_traverse_tag, T&& container, M&& mapper) ->
                typename always_void<
                    /* element_of_t<T>
                     * typename guard_traverse<
                        decltype(*std::declval<T>().begin()), M>::type,*/
                    mapped_type_from_t<T, M>>::type
            {
                for (auto&& element : std::forward<T>(container))
                {
                    std::forward<M>(mapper)(
                        std::forward<decltype(element)>(element));
                }
            }
        }    // end namespace container_remapping

        /// Provides utilities for remapping the whole content of a
        /// tuple like type to the same type holding different types.
        namespace tuple_like_remapping {
            template <typename Strategy, typename Mapper, typename T>
            struct tuple_like_remapper;

            // TODO enable only if at least one of the elements is accepted
            // in the real mapper.

            /// Specialization for std::tuple like types which contain
            /// an arbitrary amount of heterogenous arguments.
            template <typename Mapper, template <class...> class Base,
                typename... OldArgs>
            struct tuple_like_remapper<strategy_remap_tag, Mapper,
                Base<OldArgs...>>
            {
                Mapper mapper_;

                template <typename... Args>
                auto operator()(Args&&... args)
                    -> Base<typename invoke_result<Mapper, OldArgs>::type...>
                {
                    return Base<
                        typename invoke_result<Mapper, OldArgs>::type...>{
                        mapper_(std::forward<Args>(args))...};
                }
            };
            template <typename Mapper, template <class...> class Base,
                typename... OldArgs>
            struct tuple_like_remapper<strategy_traverse_tag, Mapper,
                Base<OldArgs...>>
            {
                Mapper mapper_;

                template <typename... Args>
                auto operator()(Args&&... args) -> typename always_void<
                    typename invoke_result<Mapper, OldArgs>::type...>::type
                {
                    int dummy[] = {
                        0, ((void) mapper_(std::forward<Args>(args)), 0)...};
                    (void) dummy;
                }
            };

            /// Specialization for std::array like types, which contains a
            /// compile-time known amount of homogeneous types.
            template <typename Mapper, template <class, std::size_t> class Base,
                typename OldArg, std::size_t Size>
            struct tuple_like_remapper<strategy_remap_tag, Mapper,
                Base<OldArg, Size>>
            {
                Mapper mapper_;

                template <typename... Args>
                auto operator()(Args&&... args)
                    -> Base<typename invoke_result<Mapper, OldArg>::type, Size>
                {
                    return Base<typename invoke_result<Mapper, OldArg>::type,
                        Size>{{mapper_(std::forward<Args>(args))...}};
                }
            };
            template <typename Mapper, template <class, std::size_t> class Base,
                typename OldArg, std::size_t Size>
            struct tuple_like_remapper<strategy_traverse_tag, Mapper,
                Base<OldArg, Size>>
            {
                Mapper mapper_;

                template <typename... Args>
                auto operator()(Args&&... args) -> typename invoke_result<
                    typename invoke_result<Mapper, OldArg>::type>::type
                {
                    int dummy[] = {
                        0, ((void) mapper_(std::forward<Args>(args)), 0)...};
                    (void) dummy;
                }
            };

            /// Remaps the content of the given tuple like type T,
            /// to a container of the same type which may contain
            /// different types.
            template <typename Strategy, typename T, typename M>
            auto remap(Strategy, T&& container, M&& mapper) -> decltype(
                invoke_fused(std::declval<tuple_like_remapper<Strategy,
                                 typename std::decay<M>::type, T>>(),
                    std::forward<T>(container)))
            {
                return invoke_fused(
                    tuple_like_remapper<Strategy, typename std::decay<M>::type,
                        T>{std::forward<M>(mapper)},
                    std::forward<T>(container));
            }
        }    // end namespace tuple_like_remapping

        /// Tag for dispatching based on the tuple like
        /// or container requirements
        template <bool IsContainer, bool IsTupleLike>
        struct container_match_tag
        {
        };

        template <typename T>
        using container_match_of =
            container_match_tag<traits::is_range<T>::value,
                traits::is_tuple_like<T>::value>;

        /// Base class for making strategy dependent behaviour available
        /// to the mapping_helper class.
        template <typename Strategy>
        struct mapping_strategy_base
        {
            template <typename T>
            auto may_void(T&& element) const -> typename std::decay<T>::type
            {
                return std::forward<T>(element);
            }
        };
        template <>
        struct mapping_strategy_base<strategy_traverse_tag>
        {
            template <typename MatcherTag, typename T>
            void may_void(T&& /*element*/) const
            {
            }
        };

        /// A helper class which applies the mapping or
        /// routes the element through
        template <typename Strategy, typename M>
        class mapping_helper : protected mapping_strategy_base<Strategy>
        {
            M mapper_;

            class traversal_callable_base
            {
                mapping_helper* helper_;

            public:
                explicit traversal_callable_base(mapping_helper* helper)
                  : helper_(helper)
                {
                }

            protected:
                mapping_helper* get_helper()
                {
                    return helper_;
                }
            };

            /// A callable object which forwards its invocations
            /// to mapping_helper::traverse.
            class traversor : public traversal_callable_base
            {
            public:
                using traversal_callable_base::traversal_callable_base;

                template <typename T>
                auto operator()(T&& element)
                    -> decltype(this->get_helper()->traverse(
                        Strategy{}, std::forward<T>(element)))
                {
                    return this->get_helper()->traverse(
                        Strategy{}, std::forward<T>(element));
                }

                traversor as_traversor()
                {
                    return *this;
                }
            };

            /// A callable object which forwards its invocations
            /// to mapping_helper::try_traverse.
            ///
            /// This callable object will accept any input,
            /// since elements passed to it are passed through,
            /// if the provided mapper doesn't accept it.
            class try_traversor : public traversal_callable_base
            {
            public:
                using traversal_callable_base::traversal_callable_base;

                template <typename T>
                auto operator()(T&& element)
                    -> decltype(this->get_helper()->try_traverse(
                        Strategy{}, std::forward<T>(element)))
                {
                    return this->get_helper()->try_traverse(
                        Strategy{}, std::forward<T>(element));
                }

                traversor as_traversor()
                {
                    return {this->get_helper()};
                }
            };

            /// This method implements the functionality for routing
            /// elements through, that aren't accepted by the mapper.
            /// Since the real matcher methods below are failing through SFINAE,
            /// the compiler will try to specialize this function last,
            /// since it's the least concrete one.
            /// This works recursively, so we only call the mapper
            /// with the minimal needed set of accepted arguments.
            template <typename MatcherTag, typename T>
            auto try_match(MatcherTag, T&& element)
                -> decltype(this->may_void(std::forward<T>(element)))
            {
                return this->may_void(std::forward<T>(element));
            }

            /// Match plain elements not satisfying the tuple like or
            /// container requirements.
            template <typename T>
            auto try_match(container_match_tag<false, false>, T&& element)
                -> decltype(mapper_(std::forward<T>(element)))
            {
                // T could be any non container or non tuple like type here,
                // take int or hpx::future<int> as an example.
                return mapper_(std::forward<T>(element));
            }

            /// Match elements satisfying the container requirements,
            /// which are not tuple like.
            template <typename T>
            auto try_match(container_match_tag<true, false>, T&& container)
                -> decltype(container_remapping::remap(Strategy{},
                    std::forward<T>(container), std::declval<try_traversor>()))
            {
                return container_remapping::remap(Strategy{},
                    std::forward<T>(container), try_traversor{this});
            }

            /// Match elements which are tuple like and that also may
            /// satisfy the container requirements
            /// -> We match tuple like types over container like ones
            template <bool IsContainer, typename T>
            auto try_match(
                container_match_tag<IsContainer, true>, T&& tuple_like)
                -> decltype(tuple_like_remapping::remap(Strategy{},
                    std::forward<T>(tuple_like), std::declval<try_traversor>()))
            {
                return tuple_like_remapping::remap(Strategy{},
                    std::forward<T>(tuple_like), try_traversor{this});
            }

            /// SFINAE helper for plain elements not satisfying the tuple like
            /// or container requirements.
            template <typename T>
            auto match(container_match_tag<false, false>, T&& element)
                -> decltype(mapper_(std::forward<T>(element)));

            /// SFINAE helper for elements satisfying the container
            /// requirements, which are not tuple like.
            template <typename T>
            auto match(container_match_tag<true, false>, T&& container)
                -> decltype(container_remapping::remap(Strategy{},
                    std::forward<T>(container), std::declval<traversor>()));

            /// SFINAE helper for elements which are tuple like and
            /// that also may satisfy the container requirements
            template <bool IsContainer, typename T>
            auto match(container_match_tag<IsContainer, true>, T&& tuple_like)
                -> decltype(tuple_like_remapping::remap(Strategy{},
                    std::forward<T>(tuple_like),
                    std::declval<traversor>()));

        public:
            explicit mapping_helper(M mapper)
              : mapper_(std::move(mapper))
            {
            }

            /// Traverses a single element.
            ///
            /// Doesn't allow routing through elements,
            /// that aren't accepted by the mapper
            template <typename T>
            auto traverse(Strategy, T&& element) -> decltype(this->match(
                std::declval<
                    container_match_of<typename std::decay<T>::type>>(),
                std::declval<T>()))
            {
                // We use tag dispatching here, to categorize the type T whether
                // it satisfies the container or tuple like requirements.
                // Then we can choose the underlying implementation accordingly.
                return match(container_match_of<typename std::decay<T>::type>{},
                    std::forward<T>(element));
            }

            /// \copybrief traverse
            template <typename T>
            auto try_traverse(Strategy, T&& element)
                -> decltype(this->try_match(
                    std::declval<
                        container_match_of<typename std::decay<T>::type>>(),
                    std::declval<T>()))
            {
                // We use tag dispatching here, to categorize the type T whether
                // it satisfies the container or tuple like requirements.
                // Then we can choose the underlying implementation accordingly.
                return try_match(
                    container_match_of<typename std::decay<T>::type>{},
                    std::forward<T>(element));
            }

            /// Calls the traversal method for every element in the pack,
            /// and returns a tuple containing the remapped content.
            template <typename First, typename Second, typename... T>
            auto try_traverse(strategy_remap_tag strategy, First&& first,
                Second&& second, T&&... rest)
                -> decltype(util::make_tuple(
                    try_traverse(strategy, std::forward<First>(first)),
                    try_traverse(strategy, std::forward<Second>(second)),
                    try_traverse(strategy, std::forward<T>(rest))...))
            {
                return util::make_tuple(
                    try_traverse(strategy, std::forward<First>(first)),
                    try_traverse(strategy, std::forward<Second>(second)),
                    try_traverse(strategy, std::forward<T>(rest))...);
            }

            /// Calls the traversal method for every element in the pack,
            /// without preserving the return values of the mapper.
            template <typename First, typename Second, typename... T>
            auto try_traverse(strategy_traverse_tag strategy, First&& first,
                Second&& second,
                T&&... rest) -> typename always_void<    // ...
                decltype(try_traverse(strategy, std::forward<First>(first))),
                decltype(try_traverse(strategy, std::forward<Second>(second))),
                decltype(
                    try_traverse(strategy, std::forward<T>(rest)))...>::type
            {
                try_traverse(strategy, std::forward<First>(first));
                try_traverse(strategy, std::forward<Second>(second));
                int dummy[] = {0,
                    ((void) try_traverse(strategy, std::forward<T>(rest)),
                        0)...};
                (void) dummy;
            }
        };

        /// Traverses the given pack with the given mapper and strategy
        template <typename Strategy, typename Mapper, typename... T>
        auto apply_pack_transform(
            Strategy strategy, Mapper&& mapper, T&&... pack)
            -> decltype(std::declval<mapping_helper<Strategy,
                            typename std::decay<Mapper>::type>>()
                            .try_traverse(strategy, std::forward<T>(pack)...))
        {
            mapping_helper<Strategy, typename std::decay<Mapper>::type> helper(
                std::forward<Mapper>(mapper));
            return helper.try_traverse(strategy, std::forward<T>(pack)...);
        }
    }    // end namespace detail

    /// Remaps the pack with the given mapper.
    ///
    /// TODO Detailed doc
    template <typename Mapper, typename... T>
    auto remap_pack(Mapper&& mapper, T&&... pack)
        -> decltype(detail::apply_pack_transform(detail::strategy_remap_tag{},
            std::forward<Mapper>(mapper),
            std::forward<T>(pack)...))
    {
        return detail::apply_pack_transform(detail::strategy_remap_tag{},
            std::forward<Mapper>(mapper),
            std::forward<T>(pack)...);
    }

    /// Traverses the pack with the given visitor.
    ///
    /// TODO Detailed doc
    template <typename Visitor, typename... T>
    void traverse_pack(Visitor&& visitor, T&&... pack)
    {
        detail::apply_pack_transform(detail::strategy_traverse_tag{},
            std::forward<Visitor>(visitor),
            std::forward<T>(pack)...);
    }
}    // namespace util
}    // namespace hpx

using namespace hpx;
using namespace hpx::util;
using namespace hpx::util::detail;

struct my_mapper
{
    template <typename T,
        typename std::enable_if<std::is_same<T, int>::value>::type* = nullptr>
    float operator()(T el) const
    {
        return float(el + 1.f);
    }
};

static void testTraversal()
{
    {
        auto res = remap_pack([](auto el) -> float { return float(el + 1.f); },
            0,
            1.f,
            hpx::util::make_tuple(1.f, 3),
            std::vector<std::vector<int>>{{1, 2}, {4, 5}},
            std::vector<std::vector<float>>{{1.f, 2.f}, {4.f, 5.f}},
            2);

        auto expected = hpx::util::make_tuple(    // ...
            1.f,
            2.f,
            hpx::util::make_tuple(2.f, 4.f),
            std::vector<std::vector<float>>{{2.f, 3.f}, {5.f, 6.f}},
            std::vector<std::vector<float>>{{2.f, 3.f}, {5.f, 6.f}},
            3.f);

        static_assert(std::is_same<decltype(res), decltype(expected)>::value,
            "Type mismatch!");
        HPX_TEST((res == expected));
    }

    {
        auto res = remap_pack(my_mapper{},
            0,
            1.f,
            hpx::util::make_tuple(1.f, 3,
                std::vector<std::vector<int>>{{1, 2}, {4, 5}},
                std::vector<std::vector<float>>{{1.f, 2.f}, {4.f, 5.f}}),
            2);

        auto expected = hpx::util::make_tuple(    // ...
            1.f,
            1.f,
            hpx::util::make_tuple(1.f, 4.f,
                std::vector<std::vector<float>>{{2.f, 3.f}, {5.f, 6.f}},
                std::vector<std::vector<float>>{{1.f, 2.f}, {4.f, 5.f}}),
            3.f);

        static_assert(std::is_same<decltype(res), decltype(expected)>::value,
            "Type mismatch!");
        HPX_TEST((res == expected));
    }

    {
        int count = 0;
        traverse_pack(
            [&](int el) {
                HPX_TEST_EQ((el), (count + 1));
                count = el;
            },
            1,
            hpx::util::make_tuple(
                2, 3, std::vector<std::vector<int>>{{4, 5}, {6, 7}}));

        HPX_TEST_EQ((count), (7));
    }

    return;
}

struct my_unwrapper
{
    template <typename T,
        typename std::enable_if<traits::is_future<T>::value>::type* = nullptr>
    auto operator()(T future) const -> decltype(std::declval<T>().get())
    {
        return future.get();
    }
};

static void testEarlyUnwrapped()
{
    using namespace hpx::lcos;

    {
        auto res = remap_pack(my_unwrapper{},    // ...
            0, 1, make_ready_future(3),
            make_tuple(make_ready_future(4), make_ready_future(5)));

        auto expected =
            hpx::util::make_tuple(0, 1, 3, hpx::util::make_tuple(4, 5));

        static_assert(std::is_same<decltype(res), decltype(expected)>::value,
            "Type mismatch!");
        HPX_TEST((res == expected));
    }
}

template <typename T>
struct my_allocator : public std::allocator<T>
{
    unsigned state_;

    explicit my_allocator(unsigned state)
      : state_(state)
    {
        return;
    }

    template <typename O>
    my_allocator(my_allocator<O> const& other)
      : state_(other.state_)
    {
        return;
    }
};

static void testContainerRemap()
{
    using namespace container_remapping;

    // Traits
    {
        HPX_TEST_EQ((has_push_back<std::vector<int>, int>::value), true);
        HPX_TEST_EQ((has_push_back<int, int>::value), false);
    }

    // Rebind
    {
        auto const remapper = [](unsigned short i) -> unsigned long {
            return i - 1;
        };

        // Rebinds the values
        {
            std::vector<unsigned short> source = {1, 2, 3};
            std::vector<unsigned long> dest =
                remap(strategy_remap_tag{}, source, remapper);

            HPX_TEST((dest == decltype(dest){0, 1, 2}));
        }

        // Rebinds the allocator
        {
            static unsigned const canary = 7878787;

            my_allocator<unsigned short> allocator(canary);
            std::vector<unsigned short, my_allocator<unsigned short>> source(
                allocator);
            source.push_back(1);

            // TODO Fix this test

            /*std::vector<unsigned long, my_allocator<unsigned long>> remapped =
                remap(strategy_remap_tag{}, source, remapper);*/

            // HPX_TEST_EQ((remapped.get_allocator().state_), (canary));
        }
    }
}

int main(int argc, char* argv[])
{
    testTraversal();
    testEarlyUnwrapped();
    testContainerRemap();

    auto result = hpx::util::report_errors();

    return result;
}
