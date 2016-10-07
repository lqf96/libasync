#pragma once

#include <stddef.h>
#include <sys/epoll.h>
#include <unordered_map>

namespace libasync
{   //Epoll event buffer size
    static const size_t EPOLL_EVENT_BUFFER_SIZE = 64;

    //Epoll data type
    struct EpollData
    {   //Epoll file descriptor
        int fd;
        //Epoll events buffer
        epoll_event events[EPOLL_EVENT_BUFFER_SIZE];

        //Reverse lookup table
        std::unordered_map<int, ReactorTarget*> table;
    };

    //Epoll data
    extern thread_local EpollData* epoll_data;
}
