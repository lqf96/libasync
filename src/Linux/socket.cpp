#include <errno.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <unordered_map>
#include <string>
#include <algorithm>
#include <boost/variant.hpp>
#include <libasync/socket.h>
#include <libasync/taskloop.h>

namespace libasync
{   //Epoll event buffer size
    static const size_t EPOLL_EVENT_BUFFER_SIZE = 64;
    //Socket read buffer size
    static const size_t SOCK_BUFFER_SIZE = 1024;

    //Reverse lookup table item
    typedef boost::variant<ServerSocket, Socket> TableItem;
    //Item type
    static const int SERVER_SOCKET = 0;
    static const int SOCKET = 1;

    //Epoll data type
    struct EpollData
    {   //Epoll file descriptor
        int fd;
        //Epoll events buffer
        epoll_event events[EPOLL_EVENT_BUFFER_SIZE];

        //Socket operation buffer
        char buffer[SOCK_BUFFER_SIZE];

        //Reverse lookup table
        std::unordered_map<int, TableItem> table;
    };

    //Epoll data
    static thread_local EpollData* epoll_data = nullptr;

    //Initialize socket module
    void Socket::init()
    {   //Initialize epoll data
        epoll_data = new EpollData();
        //Create epoll descriptor
        int fd = epoll_create1(0);
        if (fd==-1)
            throw SocketError(SocketError::Reason::REACTOR_INIT);
        epoll_data->fd = fd;

        //Add reactor task to task loop
        TaskLoop::thread_loop().add(Socket::reactor_task);
    }

    //Reactor task
    void Socket::reactor_task()
    {   //Wait for epoll events
        int n_events = epoll_wait(epoll_data->fd, epoll_data->events, EPOLL_EVENT_BUFFER_SIZE, -1);
        if (n_events==-1)
        {   ::close(epoll_data->fd);
            throw SocketError(SocketError::Reason::REACTOR_QUERY);
        }

        //Demultiplex events
        for (unsigned int i=0;i<n_events;i++)
        {   auto event = epoll_data->events[i];
            //Lookup socket from table
            auto item = *((TableItem*)(event.data.ptr));

            switch (item.which())
            {   //Server socket, accept incoming connections
                case SERVER_SOCKET:
                {   ServerSocket server_sock = boost::get<ServerSocket>(item);
                    int fd = server_sock.data->fd;

                    while (true)
                    {   sockaddr_in client_addr;
                        socklen_t client_addr_len = sizeof(sockaddr_in);

                        int client_fd = accept(fd, (sockaddr*)(&client_addr), &client_addr_len);
                        if (client_fd==-1)
                        {   //No more incoming connections
                            if ((errno==EAGAIN)||(errno==EWOULDBLOCK))
                                break;
                            //Error accepting incoming connection
                            else
                                throw SocketError(SocketError::Reason::ACCEPT);
                        }

                        //Create socket for incoming connection
                        Socket client_sock(client_fd);
                        auto client_data = client_sock.data;
                        //Set client local and remote address information
                        client_data->local_addr = server_sock.data->local_addr;
                        client_data->local_port = server_sock.data->local_port;
                        client_data->remote_addr = client_addr.sin_addr.s_addr;
                        client_data->remote_port = client_addr.sin_port;
                        //Trigger "connect" event
                        server_sock.trigger("connect", client_sock);
                    }

                    break;
                }
                //Client socket, handle read and write event
                case SOCKET:
                {   Socket sock = boost::get<Socket>(item);
                    auto data = sock.data;
                    int fd = data->fd;

                    //Read from socket; trigger data event
                    if (event.events&EPOLLIN)
                    {   std::string read_data;
                        ssize_t count;
                        bool closed = false;

                        while (true)
                        {   count = read(fd, epoll_data->buffer, SOCK_BUFFER_SIZE);

                            if (count==-1)
                            {   //Read error
                                if ((errno!=EAGAIN)&&(errno!=EWOULDBLOCK))
                                    throw SocketError(SocketError::Reason::READ);
                                //No more data
                                break;
                            }
                            //EOF; remote closed connection
                            else if (count==0)
                            {   closed = true;
                                break;
                            }

                            //Append to buffer
                            read_data.append(epoll_data->buffer, count);
                        }

                        //Half-closed; fully close connection
                        if (closed)
                        {   delete (TableItem*)(event.data.ptr);
                            sock.close();
                        }
                        //No more data can be read
                        else
                        {   data->bytes_read += read_data.size();
                            sock.trigger("data", read_data);
                        }
                    }
                    //Able to write or connect
                    else if (event.events&EPOLLOUT)
                    {   //Connect
                        if (data->status==Status::CONNECTING)
                        {   int result;
                            socklen_t result_len = sizeof(result);

                            //Check connection error
                            if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &result, &result_len)<0)
                                throw SocketError(SocketError::Reason::CONNECT);
                            if (result!=0)
                                throw SocketError(SocketError::Reason::CONNECT, result);

                            //Connected; trigger "connect" event
                            data->status = Status::CONNECTED;
                            sock.trigger("connect");
                        }
                        //Write
                        else
                        {   std::string& buffer = data->buffer;
                            size_t offset = 0;

                            while (true)
                            {   //Check buffer emptiness and fetch data for sending
                                if (offset>=buffer.size())
                                    break;
                                size_t write_len = std::min<size_t>(buffer.size()-offset, SOCK_BUFFER_SIZE);

                                //Try writing to socket
                                ssize_t count = ::write(fd, buffer.c_str()+offset, write_len);
                                if (count==-1)
                                {   if ((errno!=EAGAIN)&&(errno!=EWOULDBLOCK))
                                        throw SocketError(SocketError::Reason::WRITE);
                                    else
                                        break;
                                }
                                //Update offset
                                offset += count;
                            }

                            //Update write buffer and bytes written count
                            buffer = buffer.substr(buffer.size()-offset);
                            data->bytes_written += offset;
                            //Resolve write promises
                            while (true)
                            {   auto promise_item = data->write_promise_queue.front();
                                //Target not reached
                                if (promise_item.target>data->bytes_written)
                                    break;

                                //Resolve write promise
                                promise_item.ctx.resolve();
                                //Pop item from queue
                                data->write_promise_queue.pop();
                            }
                        }
                    }

                    break;
                }
            }
        }
    }

    //Register socket to reactor
    void Socket::_register()
    {   epoll_event new_event;
        int fd = this->data->fd;

        new_event.data.ptr = new TableItem(*this);
        new_event.events = EPOLLIN|EPOLLOUT|EPOLLET;
        //Add to epoll file descriptor
        if (epoll_ctl(epoll_data->fd, EPOLL_CTL_ADD, fd, &new_event)<0)
            throw SocketError(SocketError::Reason::REACTOR_REG);
    }

    //Register server socket to reactor
    void ServerSocket::_register()
    {   epoll_event new_event;
        int fd = this->data->fd;

        new_event.data.ptr = new TableItem(*this);
        new_event.events = EPOLLIN|EPOLLET;
        //Add to epoll file descriptor
        if (epoll_ctl(epoll_data->fd, EPOLL_CTL_ADD, fd, &new_event)<0)
            throw SocketError(SocketError::Reason::REACTOR_REG);
    }
}
