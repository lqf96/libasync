#include <unistd.h>
#include <sys/event.h>
#include <sys/time.h>
#include <libasync/reactor.h>
#include <libasync/taskloop.h>
#include <libasync/FreeBSD/reactor.h>

#include <cstdio>

namespace libasync
{   //Kqueue data
    thread_local KqueueData* kqueue_data = nullptr;
    //Zero time object
    const timespec zero_time = {.tv_sec = 0, .tv_nsec = 0};

    //Reactor task
    void reactor_task()
    {   //Wait for kevents
        int n_events = kevent(kqueue_data->fd, nullptr, 0, kqueue_data->events, KEVENT_BUFFER_SIZE, &zero_time);
        if (n_events==-1)
        {   ::close(kqueue_data->fd);
            throw ReactorError(ReactorError::Reason::QUERY);
        }

        //Demultiplex events
        for (unsigned int i=0;i<n_events;i++)
        {   //Event object pointer
            auto event_ptr = kqueue_data->events+i;
            //Lookup for reactor target
            auto target = kqueue_data->table[event_ptr->ident];

            //Call event handler
            target->__reactor_on_event(event_ptr);
        }
    }

    //Reactor module initialization
    void reactor_init()
    {   //Initialize kqueue data
        kqueue_data = new KqueueData();

        //Create kqueue descriptor
        int fd = kqueue();
        if (fd==-1)
            throw ReactorError(ReactorError::Reason::INIT);
        kqueue_data->fd = fd;

        //Add reactor task to task loop
        TaskLoop::thread_loop().add(reactor_task);
    }

    //Unregister object from reactor
    void reactor_unreg(int fd)
    {   //Find object associated with the file descriptor
        auto table_pair_ptr = kqueue_data->table.find(fd);
        if (table_pair_ptr==kqueue_data->table.end())
            return;

        delete table_pair_ptr->second;
        kqueue_data->table.erase(table_pair_ptr);
    }
}
