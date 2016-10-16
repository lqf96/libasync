#pragma once

#include <errno.h>
#include <exception>

namespace libasync
{   //Reactor target
    class ReactorTarget
    {protected:
        //React to event
        virtual void reactor_on_event(void* event) = 0;

        //Friend function
        friend void reactor_task();
    public:
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
