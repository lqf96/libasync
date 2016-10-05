#pragma once

#include <functional>
#include <exception>
#include <libasync/promise.h>
#include <libasync/generator.h>
#include <libasync/func_traits.h>

namespace libasync
{   //Async function helper
    template <typename AF>
    struct __AsyncHelper : public __AsyncHelper<typename FnTrait<AF>::Signature> {};

    //Await helper class
    template <typename T>
    struct __AwaitHelper
    {   static T await_impl(Promise<T> promise, Generator<void, void*> gen, GenCtx<void, void*> ctx)
        {   promise.template then<void>([=](T value) mutable
            {   gen.next((void*)(&value));
            });
            promise.template _catch<void>([=](std::exception_ptr e) mutable
            {   gen._throw(e);
            });

            auto value_ptr = (T*)(ctx.yield());
            return *value_ptr;
        }
    };

    template <>
    struct __AwaitHelper<void>
    {   static void await_impl(Promise<void> promise, Generator<void, void*> gen, GenCtx<void, void*> ctx)
        {   promise.then<void>([=]() mutable
            {   gen.next(nullptr);
            });
            promise._catch<void>([=](std::exception_ptr e) mutable
            {   gen._throw(e);
            });

            ctx.yield();
        }
    };

    //Async function context class
    class AsyncCtx
    {private:
        //Generator
        Generator<void, void*> gen;
        //Generator context
        GenCtx<void, void*> ctx;

        //Internal constructor
        AsyncCtx(Generator<void, void*> _gen, GenCtx<void, void*> _ctx) : gen(_gen), ctx(_ctx) {}

        //Friend classes
        template <typename T>
        friend class __AsyncHelper;
    public:
        //Await
        template <typename T>
        T await(Promise<T> promise)
        {   return __AwaitHelper<T>::await_impl(promise, this->gen, this->ctx);
        }
    };

    //Async function helper (Specialization)
    template <typename RT, typename... AT>
    struct __AsyncHelper<RT(AsyncCtx, AT...)>
    {   //Promise wrapped value type
        typedef typename promise::Extract<RT>::Type Value;
        //Target function type
        typedef std::function<Promise<Value>(AT...)> Func;

        //Make async function (Implementation)
        template <typename AF>
        static Func async_func_impl(AF func)
        {   return [=](AT... args)
            {   //Create a promise
                return Promise<Value>([=](PromiseCtx<Value> promise_ctx)
                {   //Create generator and get context
                    Generator<void, void*> _gen([=, &_gen](GenCtx<void, void*> gen_ctx) mutable
                    {   //Create async context
                        Generator<void, void*> gen = _gen;
                        AsyncCtx async_ctx(gen, gen_ctx);
                        //Run function
                        try
                        {   promise_ctx.resolve(func(async_ctx, args...));
                        }
                        catch (...)
                        {   promise_ctx.reject(std::current_exception());
                        }
                    });
                    //Run generator
                    _gen.next(nullptr);
                });
            };
        }
    };

    //Async function helper (Void specialization)
    template <typename... AT>
    struct __AsyncHelper<void(AsyncCtx, AT...)>
    {   //Target function type
        typedef std::function<Promise<void>(AT...)> Func;

        //Make async function (Implementation)
        template <typename AF>
        static Func async_func_impl(AF func)
        {   return [=](AT... args)
            {   //Create a promise
                return Promise<void>([=](PromiseCtx<void> promise_ctx)
                {   //Create generator and get context
                    Generator<void, void*> _gen([=, &_gen](GenCtx<void, void*> gen_ctx) mutable
                    {   //Create async context
                        Generator<void, void*> gen = _gen;
                        AsyncCtx async_ctx(gen, gen_ctx);
                        //Run function
                        try
                        {   func(async_ctx, args...);
                            promise_ctx.resolve();
                        }
                        catch (...)
                        {   promise_ctx.reject(std::current_exception());
                        }
                    });
                    //Run generator
                    _gen.next(nullptr);
                });
            };
        }
    };

    //Make async function
    template <typename AF>
    typename __AsyncHelper<AF>::Func async_func(AF func)
    {   return __AsyncHelper<AF>::async_func_impl(func);
    };
}
