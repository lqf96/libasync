#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <string>
#include <algorithm>
#include <libasync/socket.h>
#include <libasync/taskloop.h>
#include <libasync/reactor.h>
#include <libasync/Linux/reactor.h>

namespace libasync
{   //Socket buffer
    char sock_buffer[SOCK_BUFFER_SIZE];

    //Register socket to reactor
    void Socket::reactor_register()
    {   epoll_event new_event;
        int fd = this->data->fd;

        new_event.data.fd = fd;
        new_event.events = EPOLLIN|EPOLLOUT|EPOLLET;
        //Add to epoll file descriptor
        if (epoll_ctl(epoll_data->fd, EPOLL_CTL_ADD, fd, &new_event)<0)
            throw ReactorError(ReactorError::Reason::REG);

        //Add socket to lookup table
        epoll_data->table[fd] = new Socket(*this);
    }

    //Handle reactor event
    void Socket::reactor_on_event(void* _event)
    {   auto event = (epoll_event*)_event;
        auto data = this->data;

        //Read from socket; trigger data event
        if (event->events&EPOLLIN)
        {   std::string read_data;
            ssize_t count;
            bool closed = false;

            while (true)
            {   count = read(data->fd, sock_buffer, SOCK_BUFFER_SIZE);

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
                read_data.append(sock_buffer, count);
            }

            //Data received
            if (!read_data.empty())
            {   data->bytes_read += read_data.size();
                this->trigger("data", read_data);
            }

            //Peer closed connection
            if (closed)
            {   //Half-closed
                if (data->status==Status::HALF_CLOSED)
                {   //Set status and trigger "close" event
                    data->status = Status::CLOSED;
                    this->trigger("close");
                    //Unregister server socket from reactor
                    reactor_unreg(data->fd);
                }
                //Open
                else
                {   data->status = Status::HALF_CLOSED;
                    this->trigger("end");
                }
            }
        }

        //Able to write or connect
        if (event->events&EPOLLOUT)
        {   //Connect
            if (data->status==Status::CONNECTING)
            {   int result;
                socklen_t result_len = sizeof(result);

                //Check connection error
                if (getsockopt(data->fd, SOL_SOCKET, SO_ERROR, &result, &result_len)<0)
                {   this->trigger("error", SocketError(SocketError::Reason::CONNECT));
                    //Close socket and return
                    this->close();
                    return;
                }
                if (result!=0)
                {   this->trigger("error", SocketError(SocketError::Reason::CONNECT, result));
                    //Close socket and return
                    this->close();
                    return;
                }

                //Connected; trigger "connect" event
                data->status = Status::CONNECTED;
                this->trigger("connect");
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
                    ssize_t count = ::write(data->fd, buffer.c_str()+offset, write_len);
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
                buffer = buffer.substr(offset);
                data->bytes_written += offset;
                //Resolve write promises
                while (data->write_promise_queue.size()>0)
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
    }

    //Register server socket to reactor
    void ServerSocket::reactor_register()
    {   epoll_event new_event;
        int fd = this->data->fd;

        new_event.data.fd = fd;
        new_event.events = EPOLLIN|EPOLLET;
        //Add to epoll file descriptor
        if (epoll_ctl(epoll_data->fd, EPOLL_CTL_ADD, fd, &new_event)<0)
            throw ReactorError(ReactorError::Reason::REG);

        //Add server socket to lookup table
        epoll_data->table[fd] = new ServerSocket(*this);
    }

    //Handle reactor event
    void ServerSocket::reactor_on_event(void* event)
    {   auto data = this->data;

        while (true)
        {   sockaddr_in client_addr;
            socklen_t client_addr_len = sizeof(sockaddr_in);

            int client_fd = accept(data->fd, (sockaddr*)(&client_addr), &client_addr_len);
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

            //Get client local address information
            sockaddr_in addr_obj;
            socklen_t addr_obj_len = sizeof(sockaddr_in);
            //Failed to get local address and port
            if (getsockname(client_fd, (sockaddr*)(&addr_obj), &addr_obj_len)<0)
                throw SocketError(SocketError::Reason::GET_LOCAL_ADDR);
            //Set client local address
            client_data->local_addr = addr_obj.sin_addr.s_addr;
            client_data->local_port = addr_obj.sin_port;

            //Set client remote address
            client_data->remote_addr = client_addr.sin_addr.s_addr;
            client_data->remote_port = client_addr.sin_port;
            //Trigger "connect" event
            this->trigger("connect", client_sock);
        }
    }
}
