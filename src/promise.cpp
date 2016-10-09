#include <libasync/promise.h>

namespace libasync
{   namespace promise
    {   //Pending callback queue
        thread_local std::list<promise::PromiseDataBaseRef>* pending_callback_queue = nullptr;

        //Promise task
        void promise_task()
        {   //Trigger all callbacks
            for (auto data : *pending_callback_queue)
                data->call_back();
            pending_callback_queue->clear();
        }
    }

    //Initialize promise module for current thread
    void promise_init()
    {   //Initialize pending callback queue
        promise::pending_callback_queue = new std::list<promise::PromiseDataBaseRef>();
        //Add promise task to task loop
        TaskLoop::thread_loop().add(promise::promise_task);
    }
}
