#include <libasync/taskloop.h>

namespace libasync
{   //Thread task loop data
    thread_local TaskLoop::TaskLoopDataRef TaskLoop::thread_data = std::make_shared<TaskLoopData>();

    //Internal constructor
    TaskLoop::TaskLoop(TaskLoop::TaskLoopDataRef _data) : data(_data) {}

    //Constructor
    TaskLoop::TaskLoop() : TaskLoop(std::make_shared<TaskLoop::TaskLoopData>()) {}

    //Get thread loop
    TaskLoop TaskLoop::thread_loop()
    {   return TaskLoop(TaskLoop::thread_data);
    }

    //Add a permanent task to queue
    void TaskLoop::add(TaskLoop::Task task)
    {   this->data->permanent_queue.push_back(task);
    }

    //Add a oneshot task to queue
    void TaskLoop::oneshot(TaskLoop::Task task)
    {   this->data->oneshot_queue.push_back(task);
    }

    //Run loop once
    void TaskLoop::run_once()
    {   for (Task task : this->data->permanent_queue)
            task();
        for (Task task : this->data->oneshot_queue)
            task();
        this->data->oneshot_queue.clear();
    }

    //Run forever
    void TaskLoop::run()
    {   while (true)
            this->run_once();
    }

    //Synonym for "run_once()"
    void TaskLoop::operator()()
    {   this->run_once();
    }
}
