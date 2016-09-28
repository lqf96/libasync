#pragma once

#include <functional>
#include <list>
#include <memory>

namespace libasync
{   //Task loop class
    class TaskLoop
    {public:
        //Task type
        typedef std::function<void()> Task;
    private:
        //Task loop data type
        struct TaskLoopData
        {   //Permanent task queue
            std::list<Task> permanent_queue;
            //Oneshot task queue
            std::list<Task> oneshot_queue;
        };

        //Task loop data reference type
        typedef std::shared_ptr<TaskLoopData> TaskLoopDataRef;

        //Thread task loop data
        static thread_local TaskLoopDataRef thread_data;

        //Task loop data
        TaskLoopDataRef data;

        //Internal constructor
        TaskLoop(TaskLoopDataRef _data);
    public:
        //Constructor
        TaskLoop();
        //Get thread loop
        static TaskLoop thread_loop();

        //Add a permanent task to queue
        void add(Task task);
        //Add a oneshot task to queue
        void oneshot(Task task);
        //Remove task from queue (Problematic)

        //Run loop forever
        void run();
        //Run loop once
        void run_once();
        //Synonym for "run_once()"
        void operator()();
    };
}
