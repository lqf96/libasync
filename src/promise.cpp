#include <libasync/promise.h>

namespace libasync
{   namespace promise
    {   //Pending callback queue
        thread_local std::list<PromiseDataBaseRef>* pending_callback_queue = nullptr;

        //Initialize promise module for current thread
        void init()
        {   //Initialize pending callback queue
            pending_callback_queue = new std::list<PromiseDataBaseRef>();
            //Add promise task to task loop
            TaskLoop::thread_loop().add(promise_task);
        }

        //Promise task
        void promise_task()
        {   //Trigger all callbacks
            for (auto data : *pending_callback_queue)
                data->call_back();
            pending_callback_queue->clear();
        }
    }
}
