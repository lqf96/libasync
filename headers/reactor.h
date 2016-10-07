#pragma once

#include <errno.h>
#include <exception>

namespace libasync
{   //Reactor target
    class ReactorTarget
    {public:
        //React to event
        virtual void __reactor_on_event(void* event) = 0;

        //Virtual destructor
        virtual ~ReactorTarget() {}
    };

    //Reactor module initialization
    void reactor_init();
    //Reactor task
    void reactor_task();
    //Unregister object from reactor
    void reactor_unreg(int fd);

    //Reactor error type
    class ReactorError : public std::exception
    {public:
        //Reason
        enum class Reason
        {   INIT,
            QUERY,
            REG
        };

        //Constructor
        ReactorError(Reason __reason, int __error_num = errno);

        //Get reason
        Reason reason() const noexcept;
        //Get error number
        int error_num() const noexcept;
        //Get error information
        const char* what() const noexcept;
    private:
        //Reason
        Reason _reason;
        //Error number
        int _error_num;
    };
}
