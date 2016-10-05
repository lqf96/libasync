#include <libasync/taskloop.h>

namespace libasync
{   //Thread task loop data
    thread_local TaskLoop::TaskLoopDataRef TaskLoop::thread_data;

    //Constructor
    TaskLoop::TaskLoop() : data(std::make_shared<TaskLoop::TaskLoopData>()) {}

    //Internal constructor
    TaskLoop::TaskLoop(TaskLoop::TaskLoopDataRef _data) : data(_data) {}

    //Get thread loop
    TaskLoop TaskLoop::thread_loop()
    {   //Initialize thread data
        if (TaskLoop::thread_data==nullptr)
            TaskLoop::thread_data = std::make_shared<TaskLoop::TaskLoopData>();
        //Create instance
        return TaskLoop(TaskLoop::thread_data);
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

    //Get amount of permanent tasks
    size_t TaskLoop::n_permanent_tasks()
    {   return this->data->permanent_queue.size();
    }

    //Get amount of oneshot tasks
    size_t TaskLoop::n_oneshot_tasks()
    {   return this->data->oneshot_queue.size();
    }
}
