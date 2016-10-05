#pragma once

#include <ucontext.h>
#include <memory>
#include <functional>
#include <exception>
#include <boost/blank.hpp>
#include <libasync/misc.h>

namespace libasync
{   //Class declarations
    template <typename T, typename IT>
    class GenCtx;
    template <typename T, typename IT>
    class Generator;

    //Generator exception
    class GeneratorError : public std::exception
    {public:
        //Reason
        enum Reason
        {   ALREADY_RUNNING = 0,
            NOT_RUNNING = 1
        };

        //Get reason
        Reason reason() const noexcept;
        //Get error information
        const char* what() const noexcept;
    private:
        //Reason
        Reason _reason;
        //Error information
        const char* error_info;

        //Internal constructor
        GeneratorError(Reason __reason, const char* _error_info = nullptr);

        //Friend classes
        template <typename T, typename IT>
        friend class Generator;
    };

    //Generator status
    enum class GenStatus
    {   PENDING,
        RUNNING,
        SUSPENDED,
        DONE
    };

    //Generator stack size
    //(macOS requires stack size to be at least 32k)
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

        //Yield from another generator
        void yield_from(Generator<T, IT> gen)
        {   while (gen.status()!=GenStatus::DONE)
                this->yield(gen.next(*this->data->in_val));
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
            IT* in_val;
            //Exception pointer
            std::exception_ptr error;

            //Constructor
            GenData(Executor _executor = nullptr)
                : status(GenStatus::PENDING), gen_stack(nullptr), executor(_executor), in_val(nullptr) {}
            //Destructor
            ~GenData()
            {   if (this->gen_stack)
                    delete[] this->gen_stack;
            }
        };

        //Generator data reference type
        typedef std::shared_ptr<GenData> GenDataRef;

        //Generator data
        GenDataRef data;

        //Generator wrapper function
        static void gen_wrapper(GenDataRef* _data)
        {   GenDataRef data = *_data;
            //Call executor
            try
            {   data->out_val = data->executor(GenCtx<T, IT>(data));
            }
            catch (...)
            {   data->error = std::current_exception();
            }
            //Set generator status to finished and return
            data->status = GenStatus::DONE;
            swapcontext(&data->gen_ctx, data->caller_ctx);
        }

        //Yield a value (Implementation)
        static IT yield_impl(GenDataRef data, T value)
        {   //Cannot yield when generator is not running
            if (data->status!=GenStatus::RUNNING)
                throw GeneratorError(
                    GeneratorError::Reason::NOT_RUNNING,
                    "Generator is not running."
                );

            //Suspend current generator
            data->out_val = value;
            data->status = GenStatus::SUSPENDED;
            swapcontext(&data->gen_ctx, data->caller_ctx);
            //Resume
            if (data->in_val)
                return *data->in_val;
            else
            {   auto error = data->error;
                data->error = std::exception_ptr();
                std::rethrow_exception(error);
            }
        }

        //Run generator
        static void run_generator(GenDataRef data)
        {   //Switch to generator context
            ucontext_t caller_ctx;
            data->caller_ctx = &caller_ctx;
            swapcontext(&caller_ctx, &data->gen_ctx);
            //Release stack space when done
            if (data->status==GenStatus::DONE)
            {   delete[] data->gen_stack;
                data->gen_stack = nullptr;
                //Error occured
                if (data->error)
                    std::rethrow_exception(data->error);
            }
        }

        //Friend classes
        friend class GenCtx<T, IT>;
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
                {   data->in_val = &value;
                    data->status = GenStatus::RUNNING;
                    //Run generator
                    Generator<T, IT>::run_generator(data);
                    //Fall through; no break here
                }
                //Finished, return result
                case GenStatus::DONE:
                    return data->out_val;
                //Running, throw exception
                case GenStatus::RUNNING:
                    throw GeneratorError(
                        GeneratorError::Reason::ALREADY_RUNNING,
                        "Generator is already running."
                    );
            }
        }

        //Throw exception in generator
        template <typename U>
        T _throw(U error)
        {   auto data = this->data;

            //Check generator status
            switch (data->status)
            {   //Pending & finished
                case GenStatus::PENDING:
                case GenStatus::DONE:
                    throw error;
                //Suspended
                case GenStatus::SUSPENDED:
                {   data->in_val = nullptr;
                    data->error = detail::eptr_make(error);
                    data->status = GenStatus::RUNNING;
                    //Run generator
                    Generator<T, IT>::run_generator(data);
                    //Return result
                    return data->out_val;
                }
                //Running, throw exception
                case GenStatus::RUNNING:
                    throw GeneratorError(
                        GeneratorError::Reason::ALREADY_RUNNING,
                        "Generator is already running."
                    );
            }
        }

        //Get generator status
        GenStatus status()
        {   return this->data->status;
        }
    };

    //Generator context class
    template <typename IT>
    class GenCtx<void, IT>
    {private:
        //Generator context
        GenCtx<boost::blank, IT> ctx;

        //Internal constructor
        GenCtx(GenCtx<boost::blank, IT> _ctx) : ctx(_ctx) {}

        //Friend classes
        friend class Generator<void, IT>;
    public:
        //Yield a value
        IT yield()
        {   return ctx.yield(boost::blank());
        }

        //Yield from another generator (Not implemented)
    };

    //Generator class (Void type)
    template <typename IT>
    class Generator<void, IT> : protected Generator<boost::blank, IT>
    {private:
        //Executor
        typedef std::function<void(GenCtx<void, IT>)> Executor;
    public:
        //Constructor
        Generator(Executor executor) : Generator<boost::blank, IT>([=](GenCtx<boost::blank, IT> _ctx)
        {   executor(GenCtx<void, IT>(_ctx));
            return boost::blank();
        }) {}

        //Run generator
        void next(IT value)
        {   Generator<boost::blank, IT>::next(value);
        }

        //Throw exception in generator
        template <typename U>
        void _throw(U error)
        {   Generator<boost::blank, IT>::_throw(error);
        }
    };
}
