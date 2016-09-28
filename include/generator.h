#pragma once

#include <ucontext.h>
#include <memory>
#include <functional>

namespace libasync
{   //Class declarations
    template <typename T, typename IT>
    class GenCtx;
    template <typename T, typename IT>
    class Generator;

    //Generator status
    enum GenStatus
    {   PENDING = 0,
        RUNNING,
        SUSPENDED,
        DONE
    };

    //Generator stack size
#ifdef __APPLE__
    const static size_t GEN_STACK_SIZE = 32768;
#else
    const static size_t GEN_STACK_SIZE = 8192;
#endif

    //Generator context class
    template <typename T, typename IT>
    class GenCtx
    {private:
        //Generator data
        typename Generator<T, IT>::GenDataRef data;

        //Internal constructor
        GenCtx(typename Generator<T, IT>::GenDataRef _data) : data(_data) {}

        //Friend classes
        friend class Generator<T, IT>;
    public:
        //Yield a value
        IT yield(T value)
        {   return Generator<T, IT>::yield_impl(this->data, value);
        }
    };

    //Generator class
    template <typename T, typename IT>
    class Generator
    {public:
        //Executor type
        typedef std::function<T(GenCtx<T, IT>)> Executor;
    private:
        //Wrapper type
        typedef void (*Wrapper)();

        //Generator data type
        struct GenData
        {   //Status
            GenStatus status;

            //Generator context
            ucontext_t gen_ctx;
            //Generator stack
            uint8_t* gen_stack;
            //Caller context reference
            ucontext_t* caller_ctx;

            //Executor
            Executor executor;

            //Generated value
            T out_val;
            //Sent-in value
            IT in_val;

            //Constructor
            GenData(Executor _executor = nullptr)
                : status(GenStatus::PENDING), gen_stack(nullptr), executor(_executor) {}
        };

        //Generator data reference type
        typedef std::shared_ptr<GenData> GenDataRef;

        //Generator data
        GenDataRef data;

        //Generator wrapper function
        static void gen_wrapper(GenDataRef* _data)
        {   GenDataRef data = *_data;
            //Call executor
            data->out_val = data->executor(GenCtx<T, IT>(data));
            //Set generator status to finished and return
            data->status = GenStatus::DONE;
            swapcontext(&data->gen_ctx, data->caller_ctx);
        }

        //Yield a value (Implementation)
        static IT yield_impl(GenDataRef data, T value)
        {   //Suspend current generator
            data->out_val = value;
            data->status = SUSPENDED;
            swapcontext(&data->gen_ctx, data->caller_ctx);
            //Resume
            return data->in_val;
        }

        //Friend classes
        friend class GenCtx<T, IT>;
    protected:
        //Internal constructor
        Generator() {}

        //Yield a value
        IT yield(T value)
        {   return Generator<T, IT>::yield_impl(this->data, value);
        }
    public:
        //Constructor
        Generator(Executor executor) : data(std::make_shared<GenData>(executor)) {}

        //Run generator
        T next(IT value)
        {   auto data = this->data;
            ucontext_t* gen_ctx = &data->gen_ctx;

            switch (data->status)
            {   //Pending
                case GenStatus::PENDING:
                {   //Get context
                    getcontext(gen_ctx);
                    //Initialize stack space
                    data->gen_stack = new uint8_t[GEN_STACK_SIZE];
                    //Set context arguments
                    gen_ctx->uc_stack.ss_sp = data->gen_stack;
                    gen_ctx->uc_stack.ss_size = GEN_STACK_SIZE;
                    gen_ctx->uc_stack.ss_flags = 0;
                    gen_ctx->uc_link = nullptr;
                    //Make generator context
                    makecontext(gen_ctx, (Wrapper)(Generator<T, IT>::gen_wrapper), 1, &data);
                    //Fall through; no break here
                }
                //Suspended
                case GenStatus::SUSPENDED:
                {   data->in_val = value;
                    data->status = RUNNING;
                    //Switch to generator context
                    ucontext_t caller_ctx;
                    data->caller_ctx = &caller_ctx;
                    swapcontext(&caller_ctx, gen_ctx);
                    //Release stack space when done
                    if (data->status==GenStatus::DONE)
                        delete[] data->gen_stack;
                    //Fall through; no break here
                }
                //Finished, return result
                case GenStatus::DONE:
                    return data->out_val;
                //Running, throw exception
                case GenStatus::RUNNING:
                    throw "error";
            }
        }

        //Get generator status
        GenStatus status()
        {   return this->data->status;
        }
    };
}
