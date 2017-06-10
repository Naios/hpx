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
#include <hpx/util/invoke.hpp>
#include <hpx/util/invoke_fused.hpp>
#include <hpx/util/result_of.hpp>
#include <hpx/util/tuple.hpp>

// Testing
#include <hpx/util/lightweight_test.hpp>
#include <algorithm>
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

            /// Returns the type which is resulting if the mapping is applied to
            /// an element in the container.
            template <typename Container, typename Mapping>
            using mapped_type_from = typename invoke_result<Mapping,
                decltype(*(std::declval<typename std::decay<Container>::type&>()
                               .begin()))>::type;

            /// Remaps the content of the given container with type T,
            /// to a container of the same type which may contain
            /// different types.
            template <typename T, typename M>
            auto remap(T&& container, M&& mapper)
                -> decltype(rebind_container<mapped_type_from<T, M>>(container))
            {
                // TODO Maybe optimize this for the case where we map to the
                // same type, so we don't create a whole new container for
                // that case.

                // TODO void here

                // Causes errors in Clang for Windows
                //static_assert(
                //    is_back_insertable<typename std::decay<T>::type>::value,
                //    "Can only remap containers which are backward insertable!");

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

        /// A helper class which applies the mapping or routes the element through
        template <typename Strategy, typename M>
        class mapping_helper : protected mapping_strategy_base<Strategy>
        {
            M mapper_;

            struct traversor
            {
                mapping_helper* helper;

                template <typename T>
                auto operator()(T&& element) -> decltype(
                    helper->traverse(Strategy{}, std::forward<T>(element)))
                {
                    return helper->traverse(
                        Strategy{}, std::forward<T>(element));
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
            auto match(MatcherTag, T&& element)
                -> decltype(std::declval<mapping_helper*>()->may_void(
                    std::forward<T>(element)))
            {
                return this->may_void(std::forward<T>(element));
            }

            /// Match plain elements not satisfying the tuple like or
            /// container requirements.
            template <typename T>
            auto match(container_match_tag<false, false>, T&& element)
                -> decltype(mapper_(std::forward<T>(element)))
            {
                // T could be any non container or non tuple like type here,
                // take int or hpx::future<int> as an example.
                return mapper_(std::forward<T>(element));
            }

            /// Match elements satisfying the container requirements,
            /// which are not tuple like.
            template <typename T>
            auto match(container_match_tag<true, false>, T&& container)
                -> decltype(container_remapping::remap(
                    std::forward<T>(container), std::declval<traversor>()))
            {
                return container_remapping::remap(
                    std::forward<T>(container), traversor{this});
            }

            /// Match elements which are tuple like and that are also may
            /// satisfying the container requirements
            /// -> We match tuple like types over container like ones
            template <bool IsContainer, typename T>
            auto match(container_match_tag<IsContainer, true>, T&& tuple_like)
                -> decltype(tuple_like_remapping::remap(Strategy{},
                    std::forward<T>(tuple_like), std::declval<traversor>()))
            {
                return tuple_like_remapping::remap(
                    Strategy{}, std::forward<T>(tuple_like), traversor{this});
            }

        public:
            explicit mapping_helper(M mapper)
              : mapper_(std::move(mapper))
            {
            }

            /// Traverses a single element
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

            /// Calls the traversal method for every element in the pack,
            /// and returns a tuple containing the remapped content.
            template <typename First, typename Second, typename... T>
            auto traverse(strategy_remap_tag strategy, First&& first,
                Second&& second, T&&... rest)
                -> decltype(util::make_tuple(
                    traverse(strategy, std::forward<First>(first)),
                    traverse(strategy, std::forward<Second>(second)),
                    traverse(strategy, std::forward<T>(rest))...))
            {
                // TODO void here
                return util::make_tuple(
                    traverse(strategy, std::forward<First>(first)),
                    traverse(strategy, std::forward<Second>(second)),
                    traverse(strategy, std::forward<T>(rest))...);
            }

            /// Calls the traversal method for every element in the pack,
            /// without preserving the return values of the mapper.
            template <typename First, typename Second, typename... T>
            auto traverse(strategy_traverse_tag strategy, First&& first,
                Second&& second, T&&... rest) -> typename always_void<    // ...
                decltype(traverse(strategy, std::forward<First>(first))),
                decltype(traverse(strategy, std::forward<Second>(second))),
                decltype(traverse(strategy, std::forward<T>(rest)))...>::type
            {
                traverse(strategy, std::forward<First>(first));
                traverse(strategy, std::forward<Second>(second));
                int dummy[] = {0,
                    ((void) traverse(strategy, std::forward<T>(rest)), 0)...};
                (void) dummy;
            }
        };

        /// Traverses the given pack with the given mapper and strategy
        template <typename Strategy, typename Mapper, typename... T>
        auto apply_pack_transform(
            Strategy strategy, Mapper&& mapper, T&&... pack)
            -> decltype(std::declval<mapping_helper<Strategy,
                            typename std::decay<Mapper>::type>>()
                            .traverse(strategy, std::forward<T>(pack)...))
        {
            mapping_helper<Strategy, typename std::decay<Mapper>::type> helper(
                std::forward<Mapper>(mapper));
            return helper.traverse(strategy, std::forward<T>(pack)...);
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
}    // end namespace util
}    // end namespace hpx

using namespace hpx;
using namespace util;
using namespace detail;

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
        auto res = remap_pack(my_mapper{}, hpx::util::make_tuple(1.f, 3));

        int i = 0;
    }

    {
        int count = 0;
        traverse_pack(
            [&](int el) {
                HPX_TEST_EQ((el), (count + 1));
                count = el;
            },
            1, hpx::util::make_tuple(2, 3));

        HPX_TEST_EQ((count), (3));
    }

    return;
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
        HPX_TEST_EQ((is_back_insertable<std::vector<int>>::value), true);
        HPX_TEST_EQ((is_back_insertable<int>::value), false);
    }

    // Rebind
    {
        auto const remapper = [](unsigned short i) -> unsigned long {
            return i - 1;
        };

        // Rebinds the values
        {
            std::vector<unsigned short> source = {1, 2, 3};
            std::vector<unsigned long> dest = remap(source, remapper);

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
                remap(source, remapper);*/

            // HPX_TEST_EQ((remapped.get_allocator().state_), (canary));
        }
    }
}

int main(int argc, char* argv[])
{
    testTraversal();
    testContainerRemap();

    auto result = hpx::util::report_errors();

    // hpx::util::tuple<int, int>{1, 1};

    return result;
}
