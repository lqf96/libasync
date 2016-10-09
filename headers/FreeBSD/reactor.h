#pragma once

#include <stddef.h>
#include <sys/event.h>
#include <unordered_map>

namespace libasync
{   //Kqueue event buffer size
    static const size_t KEVENT_BUFFER_SIZE = 64;

    //Kqueue data type
    struct KqueueData
    {   //Kqueue file descriptor
        int fd;
        //Kqueue events buffer
        struct kevent events[KEVENT_BUFFER_SIZE];

        //Reverse lookup table
        std::unordered_map<int, ReactorTarget*> table;
    };

    //Kqueue data
    extern thread_local KqueueData* kqueue_data;
}
