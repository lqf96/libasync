#include <unistd.h>
#include <sys/event.h>
#include <libasync/reactor.h>
#include <libasync/taskloop.h>
#include <libasync/FreeBSD/reactor.h>

namespace libasync
{   //Kqueue data
    thread_local KqueueData* kqueue_data = nullptr;

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
            //(Ignore if file descriptor not in table; why is it happening?)
            auto table_pair_ptr = kqueue_data->table.find(event_ptr->ident);
            if (table_pair_ptr==kqueue_data->table.end())
                continue;

            //Call event handler
            table_pair_ptr->second->reactor_on_event(event_ptr);
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
