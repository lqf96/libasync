#include <unistd.h>
#include <sys/epoll.h>
#include <libasync/reactor.h>
#include <libasync/taskloop.h>
#include <libasync/Linux/reactor.h>

namespace libasync
{   //Epoll data
    thread_local EpollData* epoll_data = nullptr;

    //Reactor task
    void reactor_task()
    {   //Wait for epoll events
        int n_events = epoll_wait(epoll_data->fd, epoll_data->events, EPOLL_EVENT_BUFFER_SIZE, -1);
        if (n_events==-1)
        {   ::close(epoll_data->fd);
            throw ReactorError(ReactorError::Reason::QUERY);
        }

        //Demultiplex events
        for (unsigned int i=0;i<n_events;i++)
        {   //Event object pointer
            auto event_ptr = epoll_data->events+i;
            //Lookup for reactor target
            auto target = epoll_data->table[event_ptr->data.fd];

            //Call event handler
            target->__reactor_on_event(event_ptr);
        }
    }

    //Reactor module initialization
    void reactor_init()
    {   //Initialize epoll data
        epoll_data = new EpollData();

        //Create epoll descriptor
        int fd = epoll_create1(0);
        if (fd==-1)
            throw ReactorError(ReactorError::Reason::INIT);
        epoll_data->fd = fd;

        //Add reactor task to task loop
        TaskLoop::thread_loop().add(reactor_task);
    }

    //Unregister object from reactor
    void reactor_unreg(int fd)
    {   //Find object associated with the file descriptor
        auto table_pair_ptr = epoll_data->table.find(fd);
        if (table_pair_ptr==epoll_data->table.end())
            return;

        delete table_pair_ptr->second;
        epoll_data->table.erase(table_pair_ptr);
    }
}
