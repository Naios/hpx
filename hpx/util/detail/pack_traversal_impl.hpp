//  Copyright (c) 2017 Denis Blank
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef HPX_UTIL_DETAIL_PACK_TRAVERSAL_IMPL_HPP
#define HPX_UTIL_DETAIL_PACK_TRAVERSAL_IMPL_HPP

#include <hpx/config.hpp>
#include <hpx/traits/is_callable.hpp>
#include <hpx/traits/is_range.hpp>
#include <hpx/traits/is_tuple_like.hpp>
#include <hpx/util/always_void.hpp>
#include <hpx/util/detail/pack.hpp>
#include <hpx/util/invoke.hpp>
#include <hpx/util/invoke_fused.hpp>
#include <hpx/util/result_of.hpp>
#include <hpx/util/tuple.hpp>

#include <cstddef>
#include <iterator>
#include <memory>
#include <type_traits>
#include <utility>

namespace hpx {
namespace util {
    namespace detail {
        /// Exposes useful facilities for dealing with 1:n mappings
        namespace spreading {
            /// A struct to mark a tuple to be unpacked into the parent context
            template <typename... T>
            class spread_box
            {
                tuple<T...> boxed_;

            public:
                explicit spread_box(tuple<T...> boxed)
                  : boxed_(std::move(boxed))
                {
                }

                tuple<T...> unbox()
                {
                    return std::move(boxed_);
                }
            };

            /// Deduces to a true_type if the fiven type is a flat tuple
            template <typename T>
            struct is_spread : std::false_type
            {
            };
            template <typename... T>
            struct is_spread<spread_box<T...>> : std::true_type
            {
            };

            /// Converts types to the type and spread_box objects to its
            /// underlying tuple.
            template <typename T>
            T unpack(T&& type)
            {
                return std::forward<T>(type);
            }
            template <typename... T>
            auto unpack(spread_box<T...> type) -> decltype(type.unbox())
            {
                return type.unbox();
            }

            /// Deduces to the type unpack is returning when called with the
            /// the given type T.
            template <typename T>
            using unpacked_of_t = decltype(unpack(std::declval<T>()));

            /// Converts types to the type and spread_box objects to its
            /// underlying tuple. If the type is mapped to zero elements,
            /// the return type will be void.
            template <typename T>
            auto unpack_or_void(T&& type)
                -> decltype(unpack(std::forward<T>(type)))
            {
                return unpack(std::forward<T>(type));
            }
            inline void unpack_or_void(spread_box<>)
            {
            }

            /// Converts types to the a tuple carrying the single type and
            /// spread_box objects to its underlying tuple.
            template <typename T>
            auto undecorate(T&& type)
                -> decltype(make_tuple(std::forward<T>(type)))
            {
                return make_tuple(std::forward<T>(type));
            }
            template <typename... T>
            auto undecorate(spread_box<T...> type) -> decltype(type.unbox())
            {
                return type.unbox();
            }

            /// A tag for mapping the arguments to a tuple
            struct functional_tupelize_tag
            {
            };

            /// Use the recursive instantiation for a variadic pack which
            /// may contain spread types
            template <typename C, typename... T>
            auto apply_spread_impl(std::true_type, C&& callable, T&&... args)
                -> decltype(invoke_fused(std::forward<C>(callable),
                    tuple_cat(undecorate(std::forward<T>(args))...)))
            {
                return invoke_fused(std::forward<C>(callable),
                    tuple_cat(undecorate(std::forward<T>(args))...));
            }
            template <typename... T>
            auto apply_spread_impl(
                std::true_type, functional_tupelize_tag, T&&... args)
                -> decltype(tuple_cat(undecorate(std::forward<T>(args))...))
            {
                // Optimized version to prevent useless tuple
                // unpacking and re-assembling
                return tuple_cat(undecorate(std::forward<T>(args))...);
            }

            /// Use the linear instantiation for variadic packs which don't
            /// contain spread types.
            template <typename C, typename... T>
            auto apply_spread_impl(std::false_type, C&& callable, T&&... args)
                -> typename invoke_result<C, T...>::type
            {
                return invoke(
                    std::forward<C>(callable), std::forward<T>(args)...);
            }
            template <typename... T>
            tuple<T...> apply_spread_impl(
                std::false_type, functional_tupelize_tag, T&&... args)

            {
                // Optimized version to prevent useless tuple
                // unpacking and re-assembling
                return tuple<T...>{std::forward<T>(args)...};
            }

            /// Deduces to a true_type if any of the given types marks
            /// the underlying type to be spread into the current context.
            template <typename... T>
            using is_any_spread_t = any_of<is_spread<T>...>;

            template <typename C, typename... T>
            auto map_spread(C&& callable, T&&... args)
                -> decltype(apply_spread_impl(is_any_spread_t<T...>{},
                    std::forward<C>(callable), std::forward<T>(args)...))
            {
                // Check whether any of the args is a detail::flatted_tuple_t,
                // if not, use the linear called version for better
                // compilation speed.
                return apply_spread_impl(is_any_spread_t<T...>{},
                    std::forward<C>(callable), std::forward<T>(args)...);
            }

            /// Converts the given variadic arguments into a tuple in a way
            /// that spread return values are inserted into the current pack.
            template <typename... T>
            auto tupelize(T&&... args) -> decltype(
                map_spread(functional_tupelize_tag{}, std::forward<T>(args)...))
            {
                return map_spread(
                    functional_tupelize_tag{}, std::forward<T>(args)...);
            }

            /// Converts an empty tuple to void
            template <typename First, typename... Rest>
            tuple<First, Rest...> voidify_empty_tuple(tuple<First, Rest...> val)
            {
                return std::move(val);
            }
            inline void voidify_empty_tuple(tuple<>)
            {
            }

            /// Converts the given variadic arguments into a tuple in a way
            /// that spread return values are inserted into the current pack.
            ///
            /// If the returned tuple is empty, voidis returned instead.
            template <typename... T>
            auto tupelize_or_void(T&&... args) -> decltype(
                voidify_empty_tuple(tupelize(std::forward<T>(args)...)))
            {
                return voidify_empty_tuple(tupelize(std::forward<T>(args)...));
            }
        }    // end namespace spreading
    }        // end namespace detail

    /// Indicate that the result shall be spread across the parent container
    /// if possible. This can be used to create a mapper function used
    /// in map_pack that maps one element to an arbitrary count (1:n).
    template <typename... T>
    detail::spreading::spread_box<typename std::decay<T>::type...> spread_this(
        T&&... args)
    {
        return detail::spreading::spread_box<typename std::decay<T>::type...>(
            make_tuple(std::forward<T>(args)...));
    }

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

        /// Deduces to a true type if the type leads to at least one effective
        /// call to the mapper.
        template <typename Mapper, typename T>
        using is_effective_t =
            traits::is_invocable<typename Mapper::traversor_type, T>;

        /// Deduces to a true type if any type leads to at least one effective
        /// call to the mapper.
        template <typename Mapper, typename... T>
        struct is_effective_any_of_t;

        template <typename Mapper, typename First, typename... Rest>
        struct is_effective_any_of_t<Mapper, First, Rest...>
          : std::conditional<is_effective_t<Mapper, First>::value,
                std::true_type, is_effective_any_of_t<Mapper, Rest...>>::type
        {
        };
        template <typename Mapper>
        struct is_effective_any_of_t<Mapper> : std::false_type
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
                // We do nothing here, since the container doesn't
                // support reserve
            }

            /// Rebind the given allocator to NewType
            template <typename NewType, typename Allocator>
            auto rebind_allocator(Allocator&& allocator) ->
                typename std::allocator_traits<
                    Allocator>::template rebind_alloc<NewType>
            {
                return typename std::allocator_traits<Allocator>::
                    template rebind_alloc<NewType>(
                        std::forward<Allocator>(allocator));
            }

            /// Specialization for a container with a single type T
            template <typename NewType, template <class> class Base,
                typename OldType>
            auto rebind_container(Base<OldType> const & /*container*/)
                -> Base<NewType>
            {
                return Base<NewType>();
            }

            /// Specialization for a container with a single type T and
            /// a particular allocator,
            /// which is preserved across the remap.
            /// -> We remap the allocator through std::allocator_traits.
            template <typename NewType, template <class, class> class Base,
                typename OldType, typename Allocator,
                // Check whether the second argument of the container was
                // the used allocator.
                typename std::enable_if<std::uses_allocator<
                    Base<OldType, Allocator>, Allocator>::value>::type* =
                    nullptr>
            auto rebind_container(
                Base<OldType, Allocator> const& container) -> Base<NewType,
                decltype(rebind_allocator<NewType>(container.get_allocator()))>
            {
                // Create a new version of the allpcator, that is capable of
                // allocating the mapped type.
                auto allocator =
                    rebind_allocator<NewType>(container.get_allocator());
                return Base<NewType, decltype(allocator)>(std::move(allocator));
            }

            /// Returns the default iterators of the container in case
            /// the container was passed as an l-value reference.
            /// Otherwise move iterators of the container are returned.
            template <typename C, typename = void>
            class container_accessor
            {
                static_assert(std::is_lvalue_reference<C>::value,
                    "This should be a lvalue reference here!");

                C container_;

            public:
                container_accessor(C container)
                  : container_(container)
                {
                }

                auto begin() -> decltype(container_.begin())
                {
                    return container_.begin();
                }

                auto end() -> decltype(container_.end())
                {
                    return container_.end();
                }
            };
            template <typename C>
            class container_accessor<C,
                typename std::enable_if<
                    std::is_rvalue_reference<C&&>::value>::type>
            {
                C&& container_;

            public:
                container_accessor(C&& container)
                  : container_(std::move(container))
                {
                }

                auto begin()
                    -> decltype(std::make_move_iterator(container_.begin()))
                {
                    return std::make_move_iterator(container_.begin());
                }

                auto end()
                    -> decltype(std::make_move_iterator(container_.end()))
                {
                    return std::make_move_iterator(container_.end());
                }
            };

            template <typename T>
            container_accessor<T> container_accessor_of(T&& container)
            {
                // Don't use any decay here
                return container_accessor<T>(std::forward<T>(container));
            }

            /// Deduces to the type the homogeneous container is containing
            ///
            /// This alias deduces to the same type on which
            /// container_accessor<T> is iterating.
            ///
            /// The basic idea is that we deduce to the type the homogeneous
            /// container T is carrying as reference while preserving the
            /// original reference type of the container:
            /// - If the container was passed as l-value its containing
            ///   values are referenced through l-values.
            /// - If the container was passed as r-value its containing
            ///   values are referenced through r-values.
            template <typename Container>
            using element_of_t = typename std::conditional<
                std::is_rvalue_reference<Container&&>::value,
                decltype(std::move(*(std::declval<Container>().begin()))),
                decltype(*(std::declval<Container>().begin()))>::type;

            /// Removes all qualifier and references from the given type
            /// if the type is a l-value or r-value reference.
            template <typename T>
            using dereferenced_of_t =
                typename std::conditional<std::is_reference<T>::value,
                    typename std::decay<T>::type, T>::type;

            /// Returns the type which is resulting if the mapping is applied to
            /// an element in the container.
            ///
            /// Since standard containers don't allow to be instantiated with
            /// references we try to construct the container from a copied
            /// version.
            template <typename Container, typename Mapping>
            using mapped_type_from_t = dereferenced_of_t<
                spreading::unpacked_of_t<typename invoke_result<Mapping,
                    element_of_t<Container>>::type>>;

            /// We create a new container, which may hold the resulting type
            template <typename M, typename T>
            auto remap_container(std::false_type, M&& mapper, T&& container)
                -> decltype(
                    rebind_container<mapped_type_from_t<T, M>>(container))
            {
                static_assert(has_push_back<typename std::decay<T>::type,
                                  element_of_t<T>>::value,
                    "Can only remap containers, that provide a push_back "
                    "method!");

                // Create the new container, which is capable of holding
                // the remappped types.
                auto remapped =
                    rebind_container<mapped_type_from_t<T, M>>(container);

                // We try to reserve the original size from the source
                // container to the destination container.
                reserve_if_possible(
                    is_reservable<decltype(remapped)>{}, remapped, container);

                // Perform the actual value remapping from the source to
                // the destination.
                // We could have used std::transform for this, however,
                // I didn't want to pull a whole header for it in.
                for (auto&& val :
                    container_accessor_of(std::forward<T>(container)))
                {
                    remapped.push_back(spreading::unpack(std::forward<M>(
                        mapper)(std::forward<decltype(val)>(val))));
                }

                return remapped;    // RVO
            }

            /// The remapper optimized for the case that we map to the same
            /// type we accepted such as int -> int.
            template <typename M, typename T>
            auto remap_container(std::true_type, M&& mapper, T&& container) ->
                typename std::decay<T>::type
            {
                for (auto&& val :
                    container_accessor_of(std::forward<T>(container)))
                {
                    val = spreading::unpack(std::forward<M>(mapper)(
                        std::forward<decltype(val)>(val)));
                }
                return std::forward<T>(container);
            }

            /// We are allowed to reuse the container if we map to the same
            /// type we are accepting and when we have
            /// the full ownership of the container.
            template <typename T, typename M>
            using can_reuse = std::integral_constant<bool,
                std::is_same<element_of_t<T>,
                    mapped_type_from_t<T, M>>::value &&
                    std::is_rvalue_reference<T&&>::value>;

            /// Remaps the content of the given container with type T,
            /// to a container of the same type which may contain
            /// different types.
            template <typename T, typename M
#ifdef HPX_HAVE_CXX11_SFINAE_EXPRESSION_COMPLETE
                ,
                // Support for skipping completely untouched types
                typename std::enable_if<
                    is_effective_t<M, element_of_t<T>>::value>::type* = nullptr
#endif
                >
            auto remap(strategy_remap_tag, T&& container, M&& mapper)
                -> decltype(remap_container(can_reuse<T, M>{},
                    std::forward<M>(mapper), std::forward<T>(container)))
            {
                return remap_container(can_reuse<T, M>{},
                    std::forward<M>(mapper), std::forward<T>(container));
            }

            /// Just call the visitor with the content of the container
            template <typename T, typename M
#ifdef HPX_HAVE_CXX11_SFINAE_EXPRESSION_COMPLETE
                ,
                // Support for skipping completely untouched types
                typename std::enable_if<
                    is_effective_t<M, element_of_t<T>>::value>::type* = nullptr
#endif
                >
            void remap(strategy_traverse_tag, T&& container, M&& mapper)
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
            template <typename Strategy, typename Mapper, typename T
#ifdef HPX_HAVE_CXX11_SFINAE_EXPRESSION_COMPLETE
                ,
                typename = void
#endif
                >
            struct tuple_like_remapper;

            /// Specialization for std::tuple like types which contain
            /// an arbitrary amount of heterogenous arguments.
            template <typename M, template <typename...> class Base,
                typename... OldArgs>
            struct tuple_like_remapper<strategy_remap_tag, M, Base<OldArgs...>
#ifdef HPX_HAVE_CXX11_SFINAE_EXPRESSION_COMPLETE
                ,
                // Support for skipping completely untouched types
                typename std::enable_if<
                    is_effective_any_of_t<M, OldArgs...>::value>::type
#endif
                >
            {
                struct materializer
                {
                    template <typename... Args>
                    Base<Args...> operator()(Args&&... args) const
                    {
                        return {std::forward<Args>(args)...};
                    }
                };

                M mapper_;

                template <typename... Args>
                auto operator()(Args&&... args)
                    -> decltype(spreading::map_spread(materializer{},
                        std::declval<M>()(std::forward<Args>(args))...))
                {
                    return spreading::map_spread(
                        materializer{}, mapper_(std::forward<Args>(args))...);
                }
            };
            template <typename M, template <typename...> class Base,
                typename... OldArgs>
            struct tuple_like_remapper<strategy_traverse_tag, M,
                Base<OldArgs...>
#ifdef HPX_HAVE_CXX11_SFINAE_EXPRESSION_COMPLETE
                ,
                // Support for skipping completely untouched types
                typename std::enable_if<
                    is_effective_any_of_t<M, OldArgs...>::value>::type
#endif
                >
            {
                M mapper_;

                template <typename... Args>
                auto operator()(Args&&... args) -> typename always_void<
                    typename invoke_result<M, OldArgs>::type...>::type
                {
                    int dummy[] = {
                        0, ((void) mapper_(std::forward<Args>(args)), 0)...};
                    (void) dummy;
                }
            };

            /// Specialization for std::array like types, which contains a
            /// compile-time known amount of homogeneous types.
            template <typename M, template <typename, std::size_t> class Base,
                typename OldArg, std::size_t Size>
            struct tuple_like_remapper<strategy_remap_tag, M, Base<OldArg, Size>
#ifdef HPX_HAVE_CXX11_SFINAE_EXPRESSION_COMPLETE
                ,
                // Support for skipping completely untouched types
                typename std::enable_if<is_effective_t<M, OldArg>::value>::type
#endif
                >
            {
                M mapper_;

                template <typename... Args>
                auto operator()(Args&&... args)
                    -> Base<typename invoke_result<M, OldArg>::type, Size>
                {
                    return Base<typename invoke_result<M, OldArg>::type, Size>{
                        {mapper_(std::forward<Args>(args))...}};
                }
            };
            template <typename M, template <typename, std::size_t> class Base,
                typename OldArg, std::size_t Size>
            struct tuple_like_remapper<strategy_traverse_tag, M,
                Base<OldArg, Size>
#ifdef HPX_HAVE_CXX11_SFINAE_EXPRESSION_COMPLETE
                ,
                // Support for skipping completely untouched types
                typename std::enable_if<is_effective_t<M, OldArg>::value>::type
#endif
                >
            {
                M mapper_;

                template <typename... Args>
                auto operator()(Args&&... args) -> typename invoke_result<
                    typename invoke_result<M, OldArg>::type>::type
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
                                 typename std::decay<M>::type,
                                 typename std::decay<T>::type>>(),
                    std::forward<T>(container)))
            {
                return invoke_fused(
                    tuple_like_remapper<Strategy, typename std::decay<M>::type,
                        typename std::decay<T>::type>{std::forward<M>(mapper)},
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
            template <typename T>
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

                /// SFINAE helper
                template <typename T>
                auto operator()(T&& element) -> decltype(
                    std::declval<traversor>().get_helper()->traverse(
                        Strategy{}, std::forward<T>(element)));

                /// An alias to this type
                using traversor_type = traversor;
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
                auto operator()(T&& element) -> decltype(
                    std::declval<try_traversor>().get_helper()->try_traverse(
                        Strategy{}, std::forward<T>(element)))
                {
                    return this->get_helper()->try_traverse(
                        Strategy{}, std::forward<T>(element));
                }

                /// An alias to the traversor type
                using traversor_type = traversor;
            };

            /// Invokes the real mapper with the given element
            template <typename T>
            auto invoke(T&& element)
                -> decltype(std::declval<mapping_helper>().mapper_(
                    std::forward<T>(element)))
            {
                return mapper_(std::forward<T>(element));
            }

            /// SFINAE helper for plain elements not satisfying the tuple like
            /// or container requirements.
            ///
            /// We use the proxy function invoke here,
            /// because some compilers (MSVC) tend to instantiate the invocation
            /// before matching the tag, which leads to build failures.
            template <typename T>
            auto match(container_match_tag<false, false>, T&& element)
                -> decltype(std::declval<mapping_helper>().invoke(
                    std::forward<T>(element)));

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

            /// This method implements the functionality for routing
            /// elements through, that aren't accepted by the mapper.
            /// Since the real matcher methods below are failing through SFINAE,
            /// the compiler will try to specialize this function last,
            /// since it's the least concrete one.
            /// This works recursively, so we only call the mapper
            /// with the minimal needed set of accepted arguments.
            template <typename MatcherTag, typename T>
            auto try_match(MatcherTag, T&& element)
                -> decltype(std::declval<mapping_helper>().may_void(
                    std::forward<T>(element)))
            {
                return this->may_void(std::forward<T>(element));
            }

            /// Match plain elements not satisfying the tuple like or
            /// container requirements.
            ///
            /// We use the proxy function invoke here,
            /// because some compilers (MSVC) tend to instantiate the invocation
            /// before matching the tag, which leads to build failures.
            template <typename T>
            auto try_match(container_match_tag<false, false>, T&& element)
                -> decltype(std::declval<mapping_helper>().invoke(
                    std::forward<T>(element)))
            {
                // T could be any non container or non tuple like type here,
                // take int or hpx::future<int> as an example.
                return invoke(std::forward<T>(element));
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

            /// Traverses a single element.
            ///
            /// SFINAE helper: Doesn't allow routing through elements,
            /// that aren't accepted by the mapper
            template <typename T>
            auto traverse(Strategy, T&& element)
                -> decltype(std::declval<mapping_helper>().match(
                    std::declval<
                        container_match_of<typename std::decay<T>::type>>(),
                    std::declval<T>()));

            /// \copybrief traverse
            template <typename T>
            auto try_traverse(Strategy, T&& element)
                -> decltype(std::declval<mapping_helper>().try_match(
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

        public:
            explicit mapping_helper(M mapper)
              : mapper_(std::move(mapper))
            {
            }

            /// \copybrief try_traverse
            template <typename T>
            auto init_traverse(strategy_remap_tag, T&& element)
                -> decltype(spreading::unpack_or_void(
                    std::declval<mapping_helper>().try_traverse(
                        strategy_remap_tag{}, std::declval<T>())))
            {
                return spreading::unpack_or_void(try_traverse(
                    strategy_remap_tag{}, std::forward<T>(element)));
            }
            template <typename T>
            void init_traverse(strategy_traverse_tag, T&& element)
            {
                try_traverse(strategy_traverse_tag{}, std::forward<T>(element));
            }

            /// Calls the traversal method for every element in the pack,
            /// and returns a tuple containing the remapped content.
            template <typename First, typename Second, typename... T>
            auto init_traverse(strategy_remap_tag strategy, First&& first,
                Second&& second, T&&... rest)
                -> decltype(spreading::tupelize_or_void(
                    std::declval<mapping_helper>().try_traverse(
                        strategy, std::forward<First>(first)),
                    std::declval<mapping_helper>().try_traverse(
                        strategy, std::forward<Second>(second)),
                    std::declval<mapping_helper>().try_traverse(
                        strategy, std::forward<T>(rest))...))
            {
                return spreading::tupelize_or_void(
                    try_traverse(strategy, std::forward<First>(first)),
                    try_traverse(strategy, std::forward<Second>(second)),
                    try_traverse(strategy, std::forward<T>(rest))...);
            }

            /// Calls the traversal method for every element in the pack,
            /// without preserving the return values of the mapper.
            template <typename First, typename Second, typename... T>
            void init_traverse(strategy_traverse_tag strategy, First&& first,
                Second&& second, T&&... rest)
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
                            .init_traverse(strategy, std::forward<T>(pack)...))
        {
            mapping_helper<Strategy, typename std::decay<Mapper>::type> helper(
                std::forward<Mapper>(mapper));
            return helper.init_traverse(strategy, std::forward<T>(pack)...);
        }
    }    // end namespace detail
}    // end namespace util
}    // end namespace hpx

#endif    // HPX_UTIL_DETAIL_PACK_TRAVERSAL_IMPL_HPP
