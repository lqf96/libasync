#include <unistd.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <string>
#include <algorithm>
#include <libasync/socket.h>
#include <libasync/taskloop.h>
#include <libasync/reactor.h>
#include <libasync/FreeBSD/reactor.h>

namespace libasync
{   //Socket buffer
    char sock_buffer[SOCK_BUFFER_SIZE];

    //Register socket to reactor
    void Socket::reactor_register()
    {   struct kevent new_events[2];
        int fd = this->data->fd;

        //Set kevent object
        EV_SET(new_events, fd, EVFILT_READ, EV_ADD|EV_ENABLE, 0, 0, 0);
        EV_SET(new_events+1, fd, EVFILT_WRITE, EV_ADD|EV_ENABLE, 0, 0, 0);
        //Add to kqueue file descriptor
        if (kevent(kqueue_data->fd, new_events, 2, nullptr, 0, &zero_time)<0)
            throw ReactorError(ReactorError::Reason::REG);

        //Add socket to lookup table
        kqueue_data->table[fd] = new Socket(*this);
    }

    //Handle reactor event
    void Socket::__reactor_on_event(void* _event)
    {   auto event = (struct kevent*)_event;
        auto data = this->data;

        //Read from socket; trigger data event
        if (event->filter==EVFILT_READ)
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
        else if (event->filter==EVFILT_WRITE)
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
    {   struct kevent new_event;
        int fd = this->data->fd;

        //Set kevent object
        EV_SET(&new_event, fd, EVFILT_READ, EV_ADD|EV_ENABLE, 0, 0, 0);
        //Add to kqueue file descriptor
        if (kevent(kqueue_data->fd, &new_event, 1, nullptr, 0, &zero_time)<0)
            throw ReactorError(ReactorError::Reason::REG);

        //Add server socket to lookup table
        kqueue_data->table[fd] = new ServerSocket(*this);
    }

    //Handle reactor event
    void ServerSocket::__reactor_on_event(void* event)
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
            //Set client local and remote address information
            client_data->local_addr = data->local_addr;
            client_data->local_port = data->local_port;
            client_data->remote_addr = client_addr.sin_addr.s_addr;
            client_data->remote_port = client_addr.sin_port;
            //Trigger "connect" event
            this->trigger("connect", client_sock);
        }
    }
}
