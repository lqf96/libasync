#pragma once

#include <functional>
#include <memory>
#include <list>
#include <type_traits>
#include <exception>
#include <boost/blank.hpp>
#include <libasync/func_traits.h>
#include <libasync/taskloop.h>
#include <libasync/misc.h>

namespace libasync
{   //Promise context
    template <typename T>
    class PromiseCtx;
    //Promise
    template <typename T>
    class Promise;

    //Promise context (Void specialization)
    template <>
    class PromiseCtx<void>;
    //Promise (Void specialization)
    template <>
    class Promise<void>;

    //Promise namespace
    namespace promise
    {   //Promise data base type
        struct PromiseDataBase
        {   //Call callbacks
            virtual void call_back() = 0;
        };

        //Fulfilled callback helper (Definition only)
        template <typename U, typename RT, typename T, typename FF>
        struct FulfilledHelper;

        template <typename T, typename FF>
        struct FulfilledHelper<void, void, T, FF>;

        //Rejected callback helper (Definition only)
        template <typename U, typename RT, typename ET, typename RF>
        struct RejectedHelper;

        template <typename ET, typename RF>
        struct RejectedHelper<void, void, ET, RF>;

        //Promise data base reference type
        typedef std::shared_ptr<PromiseDataBase> PromiseDataBaseRef;

        //Extract wrapped type
        template <typename T>
        struct Extract
        {   typedef T Type;
        };

        template <typename T>
        struct Extract<Promise<T>>
        {   typedef T Type;
        };

        //Lift type trait
        template <typename T>
        using Lift = Promise<typename Extract<T>::Type>;

        //Pending callback queue
        extern thread_local std::list<PromiseDataBaseRef>* pending_callback_queue;

        //Promise task
        void promise_task();

        //Initialize promise module for current thread
        void init();
    };

    //Promise context class
    template <typename T>
    class PromiseCtx
    {private:
        //Promise data
        typename Promise<T>::PromiseDataRef data;

        //Internal constructor
        PromiseCtx(typename Promise<T>::PromiseDataRef _data) : data(_data) {}

        //Friend classes
        friend class Promise<T>;
    public:
        //Resolve promise
        void resolve(T value)
        {   Promise<T>::resolve_impl(this->data, value);
        }

        void resolve(Promise<T> promise)
        {   Promise<T>::resolve_impl(this->data, promise);
        }

        //Reject promise
        template <typename U>
        void reject(U error)
        {   Promise<T>::reject_impl(this->data, error);
        }
    };

    //Promise status
    enum class PromiseStatus
    {   PENDING,
        RESOLVED,
        REJECTED
    };

    //Promise class
    template <typename T>
    class Promise
    {private:
        //Promise data type (Definition)
        struct PromiseData;
        //Promise data reference type
        typedef std::shared_ptr<PromiseData> PromiseDataRef;

        //Resolve wrapper type
        typedef std::function<void(T)> ResolveWrapper;
        //Reject wrapper type
        typedef std::function<void(std::exception_ptr)> RejectWrapper;

        //Promise data type
        struct PromiseData : public promise::PromiseDataBase
        {   //Promise status
            PromiseStatus status;
            //Pending calling back
            bool pending_callback;

            //Resolved value
            T value;
            //Fulfilled wrappers
            std::list<ResolveWrapper> fulfilled_wrappers;

            //Rejected error
            std::exception_ptr error;
            //Rejected wrappers
            std::list<RejectWrapper> rejected_wrappers;

            //Call back
            void call_back()
            {   //Resolved
                if (this->status==PromiseStatus::RESOLVED)
                    for (auto wrapper : this->fulfilled_wrappers)
                        wrapper(this->value);
                //Rejected
                else if (this->status==PromiseStatus::REJECTED)
                    for (auto wrapper : this->rejected_wrappers)
                        wrapper(this->error);

                this->pending_callback = false;
                //Clear wrappers queue
                this->fulfilled_wrappers.clear();
                this->rejected_wrappers.clear();
            }
        };

        //Promise data
        PromiseDataRef data;

        //Resolve promise (Implementation)
        static void resolve_impl(PromiseDataRef data, T value)
        {   //Already settled
            if (data->status!=PromiseStatus::PENDING)
                return;

            //Set status and value
            data->status = PromiseStatus::RESOLVED;
            data->value = value;
            //Add to pending callback queue
            data->pending_callback = true;
            auto _data = std::static_pointer_cast<promise::PromiseDataBase>(data);
            promise::pending_callback_queue->push_back(_data);
        }

        static void resolve_impl(PromiseDataRef data, Promise<T> promise)
        {   //Already settled
            if (data->status!=PromiseStatus::PENDING)
                return;

            //Associate promises
            Promise<T>::associate_status(data, promise.data);
        }

        //Reject promise (Implementation)
        template <typename U>
        static void reject_impl(Promise<T>::PromiseDataRef data, U error)
        {   //Already settled
            if (data->status!=PromiseStatus::PENDING)
                return;

            //Set status and error
            data->status = PromiseStatus::REJECTED;
            data->error = detail::eptr_make(error);
            //Add to pending callback queue
            data->pending_callback = true;
            auto _data = std::static_pointer_cast<promise::PromiseDataBase>(data);
            promise::pending_callback_queue->push_back(_data);
        }

        //Associate promise status
        static void associate_status(PromiseDataRef outer_data, PromiseDataRef inner_data)
        {   switch (inner_data->status)
            {   //Resolved
                case PromiseStatus::RESOLVED:
                {   Promise<T>::resolve_impl(outer_data, inner_data->value);
                    break;
                }
                //Rejected
                case PromiseStatus::REJECTED:
                {   Promise<T>::reject_impl(outer_data, inner_data->error);
                    break;
                }
                //Pending
                case PromiseStatus::PENDING:
                {   //Internal fulfilled callback
                    inner_data->fulfilled_wrappers.push_back([=](T value)
                    {   Promise<T>::resolve_impl(outer_data, value);
                    });
                    //Internal rejected callback
                    inner_data->rejected_wrappers.push_back([=](std::exception_ptr error)
                    {   Promise<T>::reject_impl(outer_data, error);
                    });
                    break;
                }
            }
        }

        //Friend classes
        friend class PromiseCtx<T>;
        template <typename U>
        friend class Promise;
    protected:
        //Internal constructor
        Promise()
        {   auto data = this->data = std::make_shared<PromiseData>();
            data->status = PromiseStatus::PENDING;
            data->pending_callback = false;
        }

        //Resolve promise
        void resolve(T value)
        {   Promise<T>::resolve_impl(this->data, value);
        }

        void resolve(Promise<T> promise)
        {   Promise<T>::resolve_impl(this->data, promise);
        }

        //Reject promise
        template <typename U>
        void reject(U error)
        {   Promise<T>::reject_impl(this->data, error);
        }
    public:
        //Executor type
        typedef std::function<void(PromiseCtx<T>)> Executor;

        //Constructor
        Promise(Executor executor) : Promise()
        {   executor(PromiseCtx<T>(this->data));
        }

        //Construct a promise with resolved state
        static Promise<T> resolved(T value)
        {   Promise<T> promise;
            auto data = promise.data;

            data->status = PromiseStatus::RESOLVED;
            data->value = value;

            return promise;
        }

        static Promise<T> resolved(Promise<T> promise)
        {   return promise;
        }

        //Construct a promise with rejected state
        template <typename U>
        static Promise<T> rejected(U error)
        {   Promise<T> promise;
            auto data = promise.data;

            data->status = PromiseStatus::REJECTED;
            data->error = detail::eptr_make(error);

            return promise;
        }

        //Then
        template <typename U, typename FF>
        Promise<U> then(FF fulfilled)
        {   typedef typename FnTrait<FF>::ReturnType ReturnType;
            //Check function signature
            static_assert(
                std::is_same<U, typename promise::Extract<ReturnType>::Type>::value,
                "The return type of the fulfilled callback must correspond with returned promise type."
            );
            static_assert(
                FnTrait<FF>::n_args==1,
                "Fulfilled callback must have exactly one argument."
            );
            static_assert(
                std::is_same<T, typename FnTrait<FF>::template Arg<0>::Type>::value,
                "The type of the first argument of the fulfilled callback must correspond with promise type."
            );

            auto self_data = this->data;
            Promise<U> outer_promise;
            auto outer_data = outer_promise.data;

            //Rejected
            if (self_data->status==PromiseStatus::REJECTED)
                return Promise<U>();
            //Resolved
            else if ((self_data->status==PromiseStatus::RESOLVED)&&(!self_data->pending_callback))
            {   self_data->pending_callback = true;
                //Add promise to pending callback queue
                auto _self_data = std::static_pointer_cast<promise::PromiseDataBase>(self_data);
                promise::pending_callback_queue->push_back(_self_data);
            }

            //Wrap fulfilled callback and push to queue
            self_data->fulfilled_wrappers.push_back([=](T value)
            {   //Can fulfilled callback
                Promise<U> inner_promise = promise::FulfilledHelper<U, ReturnType, T, FF>::run(
                    value,
                    fulfilled
                );
                auto inner_data = inner_promise.data;

                //Associate promise status
                Promise<U>::associate_status(outer_data, inner_data);
            });

            return outer_promise;
        }

        template <typename U, typename FF, typename RF>
        Promise<U> then(FF fulfilled, RF rejected)
        {   this->then<U>(fulfilled);
            this->_catch<U>(rejected);
        }

        //Catch
        template <typename U, typename RF>
        Promise<U> _catch(RF rejected)
        {   typedef typename FnTrait<RF>::ReturnType ReturnType;
            typedef typename FnTrait<RF>::template Arg<0>::Type ErrorType;
            //Check function signature
            static_assert(
                std::is_same<U, typename promise::Extract<ReturnType>::Type>::value,
                "The return type of the rejected callback must correspond with returned promise type."
            );
            static_assert(
                FnTrait<RF>::n_args==1,
                "Rejected callback must have exactly one argument."
            );

            auto self_data = this->data;
            Promise<U> outer_promise;
            auto outer_data = outer_promise.data;

            //Resolved
            if (self_data->status==PromiseStatus::RESOLVED)
                return Promise<U>();
            //Rejected
            else if ((self_data->status==PromiseStatus::REJECTED)&&(!self_data->pending_callback))
            {   self_data->pending_callback = true;
                //Add promise to pending callback queue
                auto _self_data = std::static_pointer_cast<promise::PromiseDataBase>(self_data);
                promise::pending_callback_queue->push_back(_self_data);
            }

            //Wrap rejected callback and push to queue
            self_data->rejected_wrappers.push_back([=](std::exception_ptr error)
            {   //Can fulfilled callback
                Promise<U> inner_promise = promise::RejectedHelper<U, ReturnType, ErrorType, RF>::run(
                    detail::eptr_cast<ErrorType>(error),
                    rejected
                );
                auto inner_data = inner_promise.data;

                //Associate promise status
                Promise<U>::associate_status(outer_data, inner_data);
            });

            return outer_promise;
        }
    };

    //Promise context class (For void type)
    template <>
    class PromiseCtx<void>
    {private:
        //Wrapped promise context
        PromiseCtx<boost::blank> ctx;

        //Internal constructor
        PromiseCtx(PromiseCtx<boost::blank> _ctx) : ctx(_ctx) {}

        //Friend classes
        friend class Promise<void>;
    public:
        //Resolve promise
        void resolve()
        {   this->ctx.resolve(boost::blank());
        }

        //Buggy (Incomplete type)
        /*
        void resolve(Promise<void> promise)
        {   promise.then<void>([&]()
            {   this->ctx.resolve(boost::blank());
            }, [&](std::exception_ptr e)
            {   this->ctx.reject(e);
            });
        }
        */

        //Reject promise
        template <typename U>
        void reject(U error)
        {   this->ctx.reject(error);
        }
    };

    //Promise class (For void type)
    template <>
    class Promise<void> : protected Promise<boost::blank>
    {private:
        //Fulfilled function wrapper
        template <typename RT, typename FF>
        struct FulfilledWrapper
        {   FF& fulfilled;

            //Constructor
            FulfilledWrapper(FF& _fulfilled) : fulfilled(_fulfilled) {}

            //Wrapped fulfilled callback
            RT operator()(boost::blank _)
            {   return fulfilled();
            }
        };

        template <typename FF>
        struct FulfilledWrapper<void, FF>
        {   FF& fulfilled;

            //Constructor
            FulfilledWrapper(FF& _fulfilled) : fulfilled(_fulfilled) {}

            //Wrapped fulfilled callback
            void operator()(boost::blank _)
            {   fulfilled();
            }
        };

        //Friend classes
        friend class PromiseCtx<void>;
        template <typename T>
        friend class Promise;
    protected:
        //Internal constructor
        Promise() : Promise<boost::blank>() {}

        //Resolve promise
        void resolve()
        {   Promise<boost::blank>::resolve(boost::blank());
        }

        void resolve(Promise<void> promise)
        {   promise.then<void>([=]()
            {   this->resolve();
            });
            promise._catch<void>([=](std::exception_ptr e)
            {   this->reject(e);
            });
        }

        //Reject promise
        template <typename U>
        void reject(U error)
        {   Promise<boost::blank>::reject(error);
        }
    public:
        //Executor
        typedef std::function<void(PromiseCtx<void>)> Executor;

        //Constructor
        Promise(Executor executor) : Promise<boost::blank>([=](PromiseCtx<boost::blank> ctx)
        {   executor(PromiseCtx<void>(ctx));
        }) {}

        //Construct a promise with resolved state
        static Promise<void> resolved()
        {   Promise<void> promise;
            promise.Promise<boost::blank>::resolve(boost::blank());
            return promise;
        }

        static Promise<void> resolved(Promise<void> promise)
        {   return promise;
        }

        //Construct a promise with rejected state
        template <typename U>
        static Promise<void> rejected(U error)
        {   Promise<void> promise;
            promise.Promise<boost::blank>::reject(error);
            return promise;
        }

        //Then
        template <typename U, typename FF>
        Promise<U> then(FF fulfilled)
        {   typedef typename FnTrait<FF>::ReturnType ReturnType;
            //Check function signature
            static_assert(
                std::is_same<U, typename promise::Extract<ReturnType>::Type>::value,
                "The return type of the fulfilled callback must correspond with returned promise type."
            );
            static_assert(
                FnTrait<FF>::n_args==0,
                "Fulfilled callback must not have any arguments."
            );

            return Promise<boost::blank>::then<ReturnType>(
                FulfilledWrapper<ReturnType, FF>(fulfilled)
            );
        }

        template <typename U, typename FF, typename RF>
        Promise<U> then(FF fulfilled, RF rejected)
        {   this->then<U>(fulfilled);
            this->_catch<U>(rejected);
        }

        //Catch
        template <typename U, typename RF>
        Promise<U> _catch(RF rejected)
        {   return Promise<boost::blank>::_catch<U>(rejected);
        }
    };

    //Fulfilled callback helper
    template <typename U, typename RT, typename T, typename FF>
    struct promise::FulfilledHelper
    {   //Run fulfilled handler
        static Promise<U> run(T value, FF fulfilled)
        {   try
            {   return Promise<U>::resolved(fulfilled(value));
            }
            catch (...)
            {   return Promise<U>::rejected(std::current_exception());
            }
        }
    };

    template <typename T, typename FF>
    struct promise::FulfilledHelper<void, void, T, FF>
    {   //Run fulfilled handler
        static Promise<void> run(T value, FF fulfilled)
        {   try
            {   fulfilled(value);
                return Promise<void>::resolved();
            }
            catch (...)
            {   return Promise<void>::rejected(std::current_exception());
            }
        }
    };

    //Rejected callback helper
    template <typename U, typename RT, typename ET, typename RF>
    struct promise::RejectedHelper
    {   //Run rejected handler
        static Promise<U> run(ET error, RF rejected)
        {   try
            {   return Promise<U>::resolved(rejected(error));
            }
            catch (...)
            {   return Promise<U>::rejected(std::current_exception());
            }
        }
    };

    template <typename ET, typename RF>
    struct promise::RejectedHelper<void, void, ET, RF>
    {   //Run rejected handler
        static Promise<void> run(ET error, RF rejected)
        {   try
            {   rejected(error);
                return Promise<void>::resolved();
            }
            catch (...)
            {   return Promise<void>::rejected(std::current_exception());
            }
        }
    };
}
