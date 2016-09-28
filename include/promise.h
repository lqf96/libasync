#pragma once

#include <functional>
#include <memory>
#include <list>
#include <type_traits>
#include <boost/any.hpp>
#include <boost/blank.hpp>
#include <libasync/func_traits.h>
#include <libasync/taskloop.h>

namespace libasync
{   //Class definitions
    class PromiseModule;

    template <typename T>
    class PromiseCtx;
    template <typename T>
    class Promise;

    template <>
    class PromiseCtx<void>;
    template <>
    class Promise<void>;

    //Promise module class (Also as a helper class)
    class PromiseModule
    {private:
        //Promise data base type
        struct PromiseDataBase
        {   //Call callbacks
            virtual void call_back() = 0;
        };

        //Extract wrapped type
        template <typename T>
        struct Extract
        {   typedef T Type;
        };

        template <typename T>
        struct Extract<Promise<T>>
        {   typedef T Type;
        };

        //Promise data base reference type
        typedef std::shared_ptr<PromiseDataBase> PromiseDataBaseRef;

        //Pending callback queue
        static thread_local std::list<PromiseDataBaseRef> pending_callback_queue;

        //Promise task
        static void promise_task()
        {   //Trigger all callbacks
            for (auto data : PromiseModule::pending_callback_queue)
                data->call_back();
            PromiseModule::pending_callback_queue.clear();
        }

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

        //Friend classes
        template <typename T>
        friend class Promise;
    public:
        //Cannot be constructed
        PromiseModule() = delete;

        //Initialize promise module for current thread
        static void init();
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

        //Reject promise
        template <typename U>
        void reject(U error)
        {   Promise<T>::reject_impl(this->data, error);
        }
    };

    //Promise class
    template <typename T>
    class Promise
    {public:
        //Promise status
        enum Status
        {   PENDING = 0,
            RESOLVED,
            REJECTED
        };
    private:
        //Promise data type (Definition)
        struct PromiseData;
        //Promise data reference type
        typedef std::shared_ptr<PromiseData> PromiseDataRef;

        //Resolve wrapper type
        typedef std::function<void(T)> ResolveWrapper;
        //Reject wrapper type
        typedef std::function<void(boost::any)> RejectWrapper;

        //Promise data type
        struct PromiseData : public PromiseModule::PromiseDataBase
        {   //Promise status
            Status status;
            //Pending calling back
            bool pending_callback;

            //Resolved value
            T value;
            //Fulfilled wrappers
            std::list<ResolveWrapper> fulfilled_wrappers;

            //Rejected error
            boost::any error;
            //Rejected wrappers
            std::list<RejectWrapper> rejected_wrappers;

            //Call back
            void call_back()
            {   //Resolved
                if (this->status==Status::RESOLVED)
                    for (auto wrapper : this->fulfilled_wrappers)
                        wrapper(this->value);
                //Rejected
                else if (this->status==Status::REJECTED)
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
            if (data->status!=Status::PENDING)
                return;

            //Set status and value
            data->status = Status::RESOLVED;
            data->value = value;
            //Add to pending callback queue
            data->pending_callback = true;
            auto _data = std::static_pointer_cast<PromiseModule::PromiseDataBase>(data);
            PromiseModule::pending_callback_queue.push_back(_data);
        }

        //Reject promise (Implementation)
        template <typename U>
        static void reject_impl(Promise<T>::PromiseDataRef data, U error)
        {   //Already settled
            if (data->status!=Status::PENDING)
                return;

            //Set status and error
            data->status = Status::REJECTED;
            data->error = error;
            //Add to pending callback queue
            data->pending_callback = true;
            auto _data = std::static_pointer_cast<PromiseModule::PromiseDataBase>(data);
            PromiseModule::pending_callback_queue.push_back(_data);
        }

        //Associate promise status
        static void associate_status(PromiseDataRef outer_data, PromiseDataRef inner_data)
        {   switch (inner_data->status)
            {   //Resolved
                case Status::RESOLVED:
                {   Promise<T>::resolve_impl(outer_data, inner_data->value);
                    break;
                }
                //Rejected
                case Status::REJECTED:
                {   Promise<T>::reject_impl(outer_data, inner_data->error);
                    break;
                }
                //Pending
                case Status::PENDING:
                {   //Internal fulfilled callback
                    inner_data->fulfilled_wrappers.push_back([=](T value)
                    {   Promise<T>::resolve_impl(outer_data, value);
                    });
                    //Internal rejected callback
                    inner_data->rejected_wrappers.push_back([=](boost::any error)
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
            data->status = Status::PENDING;
            data->pending_callback = false;
        }

        //Resolve promise
        void resolve(T value)
        {   Promise<T>::resolve_impl(this->data, value);
        }

        //Reject promise
        template <typename U>
        void reject(U error)
        {   Promise<T>::reject_impl(this->data, error);
        }
    public:
        //Construct a promise by calling given executor
        template <typename EF>
        static Promise<T> create(EF executor)
        {   //Check function signature
            static_assert(
                std::is_same<void, typename FnTrait<EF>::ReturnType>::value,
                "The return type of the executor must be void."
            );
            static_assert(
                FnTrait<EF>::n_args==1,
                "The executor must have exactly one argument."
            );
            static_assert(
                std::is_same<PromiseCtx<T>, typename FnTrait<EF>::template Arg<0>::Type>::value,
                "The type of the first argument of the executor must be PromiseCtx<T>."
            );

            Promise<T> promise;
            executor(PromiseCtx<T>(promise.data));
            return promise;
        }

        //Construct a promise with resolved state
        static Promise<T> resolved(T value)
        {   Promise<T> promise;
            auto data = promise.data;

            data->status = Status::RESOLVED;
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

            data->status = Status::REJECTED;
            data->error = error;

            return promise;
        }

        //Then
        template <typename U, typename FF>
        Promise<U> then(FF fulfilled)
        {   typedef typename FnTrait<FF>::ReturnType ReturnType;
            //Check function signature
            static_assert(
                std::is_same<U, typename PromiseModule::Extract<ReturnType>::Type>::value,
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
            if (self_data->status==Status::REJECTED)
                return Promise<U>();
            //Resolved
            else if ((self_data->status==Status::RESOLVED)&&(!self_data->pending_callback))
            {   self_data->pending_callback = true;
                //Add promise to pending callback queue
                auto _self_data = std::static_pointer_cast<PromiseModule::PromiseDataBase>(self_data);
                PromiseModule::pending_callback_queue.push_back(_self_data);
            }

            //Wrap fulfilled callback and push to queue
            self_data->fulfilled_wrappers.push_back([=](T value)
            {   //Can fulfilled callback
                Promise<U> inner_promise = PromiseModule::FulfilledHelper<U, ReturnType, T, FF>::run(
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
                std::is_same<U, typename PromiseModule::Extract<ReturnType>::Type>::value,
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
            if (self_data->status==Status::RESOLVED)
                return Promise<U>();
            //Rejected
            else if ((self_data->status==Status::REJECTED)&&(!self_data->pending_callback))
            {   self_data->pending_callback = true;
                //Add promise to pending callback queue
                auto _self_data = std::static_pointer_cast<PromiseModule::PromiseDataBase>(self_data);
                PromiseModule::pending_callback_queue.push_back(_self_data);
            }

            //Wrap rejected callback and push to queue
            self_data->rejected_wrappers.push_back([=](boost::any error)
            {   //Can fulfilled callback
                Promise<U> inner_promise = PromiseModule::RejectedHelper<U, ReturnType, ErrorType, RF>::run(
                    boost::any_cast<ErrorType>(error),
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
        //Empty object
        static boost::blank empty_obj;

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

        //Reject promise
        template <typename U>
        void reject(U error)
        {   Promise<boost::blank>::reject(error);
        }
    public:
        //Construct a promise by calling given executor
        template <typename EF>
        static Promise<void> create(EF executor)
        {   //Check function signature
            static_assert(
                std::is_same<void, typename FnTrait<EF>::ReturnType>::value,
                "The return type of the executor must be void."
            );
            static_assert(
                FnTrait<EF>::n_args==1,
                "The executor must have exactly one argument."
            );
            static_assert(
                std::is_same<PromiseCtx<void>, typename FnTrait<EF>::template Arg<0>::Type>::value,
                "The type of the first argument of the executor must be PromiseCtx<void>."
            );

            Promise<void> promise([&](PromiseCtx<boost::blank> ctx)
            {   executor(PromiseCtx<void>(ctx));
            });
            return promise;
        }

        //Construct a promise with resolved state
        static Promise<void> resolved()
        {   Promise<void> promise;
            promise.Promise<boost::blank>::resolve(Promise<void>::empty_obj);
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
                std::is_same<U, typename PromiseModule::Extract<ReturnType>::Type>::value,
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
        {   Promise<boost::blank>::_catch(rejected);
        }
    };

    //Fulfilled callback helper
    template <typename U, typename RT, typename T, typename FF>
    struct PromiseModule::FulfilledHelper
    {   //Run fulfilled handler
        static Promise<U> run(T value, FF fulfilled)
        {   return Promise<U>::resolved(fulfilled(value));
        }
    };

    template <typename T, typename FF>
    struct PromiseModule::FulfilledHelper<void, void, T, FF>
    {   //Run fulfilled handler
        static Promise<void> run(T value, FF fulfilled)
        {   fulfilled(value);
            return Promise<void>::resolved();
        }
    };

    //Rejected callback helper
    template <typename U, typename RT, typename ET, typename RF>
    struct PromiseModule::RejectedHelper
    {   //Run rejected handler
        static Promise<U> run(ET error, RF rejected)
        {   return Promise<U>::resolved(rejected(error));
        }
    };

    template <typename ET, typename RF>
    struct PromiseModule::RejectedHelper<void, void, ET, RF>
    {   //Run rejected handler
        static Promise<void> run(ET error, RF rejected)
        {   rejected(error);
            return Promise<void>::resolved();
        }
    };
}
