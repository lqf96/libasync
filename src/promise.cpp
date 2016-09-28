#include <libasync/promise.h>

namespace libasync
{   //Pending callback queue
    thread_local std::list<PromiseModule::PromiseDataBaseRef> PromiseModule::pending_callback_queue;

    //Initialize promise module for current thread
    void PromiseModule::init()
    {   TaskLoop::thread_loop().add(PromiseModule::promise_task);
    }
}
