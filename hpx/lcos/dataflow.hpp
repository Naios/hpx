//  Copyright (c) 2007-2017 Hartmut Kaiser
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// hpxinspect:nodeprecatedinclude:boost/ref.hpp
// hpxinspect:nodeprecatedname:boost::reference_wrapper

#ifndef HPX_LCOS_DATAFLOW_HPP
#define HPX_LCOS_DATAFLOW_HPP

#include <hpx/config.hpp>
#include <hpx/apply.hpp>
#include <hpx/lcos/future.hpp>
#include <hpx/runtime/get_worker_thread_num.hpp>
#include <hpx/runtime/launch_policy.hpp>
#include <hpx/traits/acquire_future.hpp>
#include <hpx/traits/extract_action.hpp>
#include <hpx/traits/future_access.hpp>
#include <hpx/traits/future_traits.hpp>
#include <hpx/traits/is_action.hpp>
#include <hpx/traits/is_distribution_policy.hpp>
#include <hpx/traits/is_executor.hpp>
#include <hpx/traits/is_future.hpp>
#include <hpx/traits/is_future_range.hpp>
#include <hpx/traits/is_launch_policy.hpp>
#include <hpx/traits/promise_local_result.hpp>
#include <hpx/util/annotated_function.hpp>
#include <hpx/util/deferred_call.hpp>
#include <hpx/util/detail/pack.hpp>
#include <hpx/util/invoke_fused.hpp>
#include <hpx/util/range.hpp>
#include <hpx/util/thread_description.hpp>
#include <hpx/util/tuple.hpp>
#include <hpx/util/unwrap_ref.hpp>

#if defined(HPX_HAVE_EXECUTOR_COMPATIBILITY)
#include <hpx/traits/is_executor_v1.hpp>
#include <hpx/parallel/executors/executor_traits.hpp>
#endif
#include <hpx/parallel/executors/execution.hpp>

#include <boost/atomic.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/ref.hpp>

#include <cstddef>
#include <exception>
#include <functional>
#include <iterator>
#include <type_traits>
#include <utility>

///////////////////////////////////////////////////////////////////////////////
namespace hpx { namespace lcos { namespace detail
{
    // dispatch point used for dataflow implementations
    template <typename Func, typename Enable = void>
    struct dataflow_dispatch;

    // dispatch point used for dataflow<Action> implementations
    template <typename Action, typename Policy, typename Enable = void>
    struct dataflow_action_dispatch;

    // dispatch point used for launch_policy implementations
    template <typename Action, typename Enable = void>
    struct dataflow_launch_policy_dispatch;

    ///////////////////////////////////////////////////////////////////////////
    struct reset_dataflow_future
    {
        template <typename Future>
        HPX_FORCEINLINE
        typename std::enable_if<
            traits::detail::is_future_or_future_range<Future>::value
        >::type operator()(Future& future) const
        {
            future = Future();
        }

        template <typename Future>
        HPX_FORCEINLINE
        typename std::enable_if<
            traits::detail::is_future_or_future_range<Future>::value
        >::type operator()(boost::reference_wrapper<Future>& future) const
        {
            future.get() = Future();
        }

        template <typename Future>
        HPX_FORCEINLINE
        typename std::enable_if<
            traits::detail::is_future_or_future_range<Future>::value
        >::type operator()(std::reference_wrapper<Future>& future) const
        {
            future.get() = Future();
        }

        template <typename Future>
        HPX_FORCEINLINE
        typename std::enable_if<
            !traits::detail::is_future_or_future_range<Future>::value
        >::type operator()(Future& future) const
        {}
    };

    template<typename T>
    void invalidate_futures(T&& futures)
    {
        /*
         reset_dataflow_future reset;
                int const _sequencer[] = {
                    ((reset(util::get<Is>(futures_))), 0)...
                };
         */
    }

    ///////////////////////////////////////////////////////////////////////////
    template <typename F, typename Args, typename Enable = void>
    struct dataflow_return;

    template <typename F, typename Args>
    struct dataflow_return<F, Args,
        typename std::enable_if<!traits::is_action<F>::value>::type
    > : util::detail::invoke_fused_result<F, Args>
    {};

    template <typename Action, typename Args>
    struct dataflow_return<Action, Args,
        typename std::enable_if<traits::is_action<Action>::value>::type
    >
    {
        typedef typename Action::result_type type;
    };

    ///////////////////////////////////////////////////////////////////////////
    template <typename Policy, typename Func, typename Futures>
    struct dataflow_frame //-V690
      : hpx::lcos::detail::future_data<
            typename detail::dataflow_return<Func, Futures>::type>
    {
        typedef
            typename detail::dataflow_return<Func, Futures>::type
            result_type;
        typedef hpx::lcos::detail::future_data<result_type> base_type;

        typedef hpx::lcos::future<result_type> type;

        using is_void_result = std::is_void<result_type>;

        // TODO Remove
        typedef typename util::detail::make_index_pack<
                util::tuple_size<Futures>::value
            >::type indices_type;

        // TODO Remove
        template <std::size_t I>
        struct is_end
          : std::integral_constant<
                bool,
                util::tuple_size<Futures>::value == I
            >
        {};

        // TODO Remove
        typedef typename std::conditional<
                is_void_result::value
              , void(dataflow_frame::*)(indices_type, std::true_type)
              , void(dataflow_frame::*)(indices_type, std::false_type)
            >::type execute_function_type;

    private:
        // workaround gcc regression wrongly instantiating constructors
        dataflow_frame();
        dataflow_frame(dataflow_frame const&);

    public:
        typedef typename base_type::init_no_addref init_no_addref;

        /// A struct to construct the dataflow_frame in-place
        template<bool ShouldInitNoAddref>
        struct construction_data
        {
            Policy policy_;
            Func func_;
        };

        dataflow_frame(construction_data<false> data)
          : policy_(std::move(data.policy_))
          , func_(std::move(data.func_))
        {
        }

        dataflow_frame(construction_data<true> data)
          : base_type(init_no_addref{})
          , policy_(std::move(data.policy_))
          , func_(std::move(data.func_))
        {
        }

    protected:
        ///////////////////////////////////////////////////////////////////////
        /// Passes the futures into the evaluation function and
        /// sets the result future.
        HPX_FORCEINLINE
        void execute(std::false_type, Futures&& futures)
        {
            try {
                result_type res =
                    util::invoke_fused(func_, std::move(futures));

                // Bad!
                // invalidate_futures(futures_);

                this->set_data(std::move(res));
            }
            catch(...) {
                this->set_exception(std::current_exception());
            }
        }

        /// Passes the futures into the evaluation function and
        /// sets the result future.
        HPX_FORCEINLINE
        void execute(std::true_type, Futures&& futures)
        {
            try {
                util::invoke_fused(func_, std::move(futures));

                // Bad!
                // invalidate_futures(futures_);

                this->set_data(util::unused_type());
            }
            catch(...) {
                this->set_exception(std::current_exception());
            }
        }

        HPX_FORCEINLINE void done(Futures futures)
        {
            hpx::util::annotate_function annotate(func_);

            execute(is_void_result{}, std::move(futures));
        }

        ///////////////////////////////////////////////////////////////////////
        void finalize(hpx::detail::async_policy policy, Futures&& futures)
        {
            // schedule the final function invocation with high priority
            util::thread_description desc(func_, "dataflow_frame::finalize");
            boost::intrusive_ptr<dataflow_frame> this_(this);

            // simply schedule new thread
            threads::register_thread_nullary(
                util::deferred_call(&dataflow_frame::done, std::move(this_))
              , desc
              , threads::pending
              , true
              , policy.priority()
              , std::size_t(-1)
              , threads::thread_stacksize_current);
        }

        HPX_FORCEINLINE
        void finalize(hpx::detail::sync_policy)
        {
            done();
        }

        void finalize(hpx::detail::fork_policy policy)
        {
            // schedule the final function invocation with high priority
            util::thread_description desc(func_, "dataflow_frame::finalize");
            boost::intrusive_ptr<dataflow_frame> this_(this);

            threads::thread_id_type tid = threads::register_thread_nullary(
                util::deferred_call(&dataflow_frame::done, std::move(this_))
              , desc
              , threads::pending_do_not_schedule
              , true
              , policy.priority()
              , get_worker_thread_num()
              , threads::thread_stacksize_current);

            if (tid)
            {
                // make sure this thread is executed last
                hpx::this_thread::yield_to(thread::id(std::move(tid)));
            }
        }

        void finalize(launch policy)
        {
            if (policy == launch::sync)
            {
                finalize(launch::sync);
            }
            else if (policy == launch::fork)
            {
                finalize(launch::fork);
            }
            else
            {
                finalize(launch::async);
            }
        }

        HPX_FORCEINLINE
        void finalize(threads::executor& sched)
        {
            boost::intrusive_ptr<dataflow_frame> this_(this);
            hpx::apply(sched, &dataflow_frame::done, std::move(this_));
        }

#if defined(HPX_HAVE_EXECUTOR_COMPATIBILITY)
        // handle executors through their executor_traits
        template <typename Executor>
        HPX_DEPRECATED(HPX_DEPRECATED_MSG) HPX_FORCEINLINE
        typename std::enable_if<
            traits::is_executor<Executor>::value
        >::type
        finalize(Executor& exec)
        {
            boost::intrusive_ptr<dataflow_frame> this_(this);
            parallel::executor_traits<Executor>::apply_execute(exec,
                &dataflow_frame::done, std::move(this_));
        }
#endif

        template <typename Executor>
        HPX_FORCEINLINE
        typename std::enable_if<
            traits::is_one_way_executor<Executor>::value ||
            traits::is_two_way_executor<Executor>::value
        >::type
        finalize(Executor& exec)
        {
            execute_function_type f = &dataflow_frame::execute;
            boost::intrusive_ptr<dataflow_frame> this_(this);

            parallel::execution::post(exec,
                f, std::move(this_), indices_type(), is_void());
        }

    private:
        Policy policy_;
        Func func_;
    };

    ///////////////////////////////////////////////////////////////////////////
    // any action
    template <typename Action>
    struct dataflow_dispatch<Action,
        typename std::enable_if<traits::is_action<Action>::value>::type>
    {
        template <typename Policy,
            typename Component, typename Signature, typename Derived,
            typename ...Ts>
        HPX_FORCEINLINE static
        typename std::enable_if<
            traits::is_launch_policy<
                typename std::decay<Policy>::type
            >::value,
            typename dataflow_frame<
                typename std::decay<Policy>::type
              , Derived
              , util::tuple<
                    hpx::id_type
                  , typename traits::acquire_future<Ts>::type...
                >
            >::type
        >::type
        call(Policy && launch_policy,
            hpx::actions::basic_action<Component, Signature, Derived> const& act,
            naming::id_type const& id, Ts &&... ts)
        {
            typedef
                dataflow_frame<
                    typename std::decay<Policy>::type
                  , Derived
                  , util::tuple<
                        hpx::id_type
                      , typename traits::acquire_future<Ts>::type...
                    >
                >
                frame_type;
            typedef typename frame_type::init_no_addref init_no_addref;

            boost::intrusive_ptr<frame_type> p(new frame_type(
                    std::forward<Policy>(launch_policy)
                  , Derived()
                  , util::forward_as_tuple(
                        id
                      , traits::acquire_future_disp()(std::forward<Ts>(ts))...
                    )
                  , init_no_addref()
                ), false);
            p->do_await();

            using traits::future_access;
            return future_access<typename frame_type::type>::create(std::move(p));
        }

        template <
            typename Component, typename Signature, typename Derived,
            typename ...Ts>
        HPX_FORCEINLINE static
        typename dataflow_frame<
            hpx::detail::async_policy
          , Derived
          , util::tuple<
                hpx::id_type
              , typename traits::acquire_future<Ts>::type...
            >
        >::type
        call(hpx::actions::basic_action<Component, Signature, Derived> const& act,
            naming::id_type const& id, Ts &&... ts)
        {
            return call(launch::async, act, id, std::forward<Ts>(ts)...);
        }
    };

    // launch
    template <typename Action, typename Policy>
    struct dataflow_action_dispatch<Action, Policy,
        typename std::enable_if<
            traits::is_launch_policy<typename std::decay<Policy>::type>::value
        >::type>
    {
        template <typename Policy_, typename ...Ts>
        HPX_FORCEINLINE static lcos::future<
            typename traits::promise_local_result<
                typename hpx::traits::extract_action<
                    Action
                >::remote_result_type
            >::type>
        call(Policy_ && launch_policy, naming::id_type const& id, Ts &&... ts)
        {
            return dataflow_dispatch<Action>::call(
                std::forward<Policy_>(launch_policy), Action(), id,
                std::forward<Ts>(ts)...);
        }

//         template <typename DistPolicy, typename ...Ts>
//         HPX_FORCEINLINE static
//         typename std::enable_if<
//             traits::is_distribution_policy<DistPolicy>::value,
//             lcos::future<
//                 typename traits::promise_local_result<
//                     typename hpx::traits::extract_action<
//                         Action
//                     >::remote_result_type
//                 >::type
//             >
//         >::type
//         call(Policy launch_policy, DistPolicy const& policy, Ts&&... ts)
//         {
//             return policy.template async<Action>(launch_policy,
//                 std::forward<Ts>(ts)...);
//         }
    };

    // naming::id_type
    template <typename Action>
    struct dataflow_action_dispatch<Action, naming::id_type>
    {
        template <typename ...Ts>
        HPX_FORCEINLINE static
        lcos::future<
            typename traits::promise_local_result<
                typename hpx::traits::extract_action<
                    Action
                >::remote_result_type
            >::type>
        call(naming::id_type const& id, Ts&&... ts)
        {
            return dataflow_action_dispatch<
                    Action, hpx::detail::async_policy
                >::call(launch::async, id, std::forward<Ts>(ts)...);
        }
    };

    // distribution policy
//     template <typename Action, typename Policy>
//     struct dataflow_action_dispatch<Action, Policy,
//         typename std::enable_if<
//             traits::is_distribution_policy<Policy>::value
//         >::type>
//     {
//         template <typename DistPolicy, typename ...Ts>
//         HPX_FORCEINLINE static
//         lcos::future<
//             typename traits::promise_local_result<
//                 typename hpx::traits::extract_action<
//                     Action
//                 >::remote_result_type
//             >::type>
//         call(DistPolicy const& policy, Ts&&... ts)
//         {
//             return dataflow_action_dispatch<
//                     Action, hpx::detail::async_policy
//                 >::call(launch::async, policy, std::forward<Ts>(ts)...);
//         }
//     };

    ///////////////////////////////////////////////////////////////////////////
    template <typename Action>
    struct dataflow_launch_policy_dispatch<Action,
        typename std::enable_if<traits::is_action<Action>::value>::type>
    {
        typedef typename traits::promise_local_result<
                typename hpx::traits::extract_action<
                    Action
                >::remote_result_type
            >::type result_type;

        template <typename Policy, typename... Ts>
        HPX_FORCEINLINE static lcos::future<result_type>
        call(Policy && launch_policy, Action const&, naming::id_type const& id,
            Ts&&... ts)
        {
            static_assert(
                traits::is_launch_policy<
                    typename std::decay<Policy>::type
                >::value,
                "Policy must be a valid launch policy");

            return dataflow_action_dispatch<
                    Action, launch
                >::call(std::forward<Policy>(launch_policy), id,
                    std::forward<Ts>(ts)...);
        }

//         template <typename Policy, typename DistPolicy, typename ...Ts>
//         HPX_FORCEINLINE static
//         typename std::enable_if<
//             traits::is_distribution_policy<DistPolicy>::value,
//             lcos::future<result_type>
//         >::type
//         call(Policy launch_policy, Action const&, DistPolicy const& policy,
//             Ts&&... ts)
//         {
//             static_assert(traits::is_launch_policy<Policy>::value,
//                 "Policy must be a valid launch policy");
//
//             return async<Action>(launch_policy, policy, std::forward<Ts>(ts)...);
//         }
    };
}}}

///////////////////////////////////////////////////////////////////////////////
// distributed dataflow: invokes action when ready
namespace hpx
{
    template <typename Action, typename F, typename ...Ts>
    HPX_FORCEINLINE
    auto dataflow(F && f, Ts &&... ts)
    ->  decltype(lcos::detail::dataflow_action_dispatch<
                    Action, typename std::decay<F>::type
            >::call(std::forward<F>(f), std::forward<Ts>(ts)...))
    {
        return lcos::detail::dataflow_action_dispatch<
                typename std::decay<Action>::type, typename std::decay<F>::type
            >::call(std::forward<F>(f), std::forward<Ts>(ts)...);
    }
}

#endif /*HPX_LCOS_DATAFLOW_HPP*/
