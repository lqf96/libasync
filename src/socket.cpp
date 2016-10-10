#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <algorithm>
#include <libasync/socket.h>

namespace libasync
{   //Buffer size
    static const size_t N_BYTES_PER_ROUND = 4096;

    //Socket exception constructor
    SocketError::SocketError(Reason __reason, int __error_num)
        : _reason(__reason), _error_num(__error_num) {}

    //Get reason
    SocketError::Reason SocketError::reason() const noexcept
    {   return this->_reason;
    }

    //Get error number
    int SocketError::error_num() const noexcept
    {   return this->_error_num;
    }

    //Get error information
    const char* SocketError::what() const noexcept
    {   return strerror(this->_error_num);
    }

    //Socket constructor
    Socket::Socket() : data(std::make_shared<SocketData>())
    {   //Create socket file descriptor
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd==-1)
            throw SocketError(SocketError::Reason::CREATE);
        this->data->fd = fd;

        this->create();
    }

    //Socket internal constructor
    Socket::Socket(int fd) : data(std::make_shared<SocketData>())
    {   this->data->fd = fd;
        this->data->status = Status::CONNECTED;

        this->create();
    }

    //Shared socket initialization logic
    void Socket::create()
    {   int fd = this->data->fd;

        //Make socket non-block
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags==-1)
            throw SocketError(SocketError::Reason::MAKE_NON_BLOCK);
        flags |= O_NONBLOCK;
        if (fcntl(fd, F_SETFL, flags)<0)
            throw SocketError(SocketError::Reason::MAKE_NON_BLOCK);

        //Register socket to reactor
        this->reactor_register();
    }

    //Get local address and port
    bool Socket::local_addr(in_addr_t* addr, in_port_t* port)
    {   auto data = this->data;

        //Not connected; do nothing
        if (data->status!=Status::CONNECTED)
            return false;
        //Local address not obtained
        if (data->local_addr==INADDR_NONE)
        {   sockaddr_in addr_obj;
            socklen_t addr_obj_len = sizeof(sockaddr_in);

            //Failed to get local address and port
            if (getsockname(data->fd, (sockaddr*)(&addr_obj), &addr_obj_len)<0)
                throw SocketError(SocketError::Reason::GET_LOCAL_ADDR);

            data->local_addr = addr_obj.sin_addr.s_addr;
            data->local_port = addr_obj.sin_port;
        }
        //Copy local address and port
        *addr = data->local_addr;
        *port = data->local_port;
        return true;
    }

    //Get remote address and port
    bool Socket::remote_addr(in_addr_t* addr, in_port_t* port)
    {   auto data = this->data;

        //Not connected; do nothing
        if (data->status!=Status::CONNECTED)
            return false;
        //Copy remote address and port
        *addr = data->remote_addr;
        *port = data->remote_port;
        return true;
    }

    //Bind to given address and port
    void Socket::bind(in_addr_t addr, in_port_t port)
    {   auto data = this->data;
        if ((data->status!=Status::IDLE)||(data->local_addr!=INADDR_NONE))
            return;

        //Make socket address object
        sockaddr_in addr_obj;
        addr_obj.sin_family = AF_INET;
        addr_obj.sin_addr.s_addr = addr;
        addr_obj.sin_port = port;
        //Bind socket to given address
        if (::bind(data->fd, (sockaddr*)(&addr_obj), sizeof(sockaddr_in))<0)
            throw SocketError(SocketError::Reason::BIND);

        //Set local address and port
        data->local_addr = addr;
        data->local_port = port;
    }

    //Connect to given address and port
    Promise<void> Socket::connect(in_addr_t addr, in_port_t port)
    {   auto data = this->data;
        bool connected = true;
        //Already connected; do nothing
        if (data->status!=Status::IDLE)
            return Promise<void>::resolved();

        //Construct address object
        sockaddr_in addr_obj;
        addr_obj.sin_family = AF_INET;
        addr_obj.sin_addr.s_addr = addr;
        addr_obj.sin_port = port;
        //Try to connect to remote
        if (::connect(data->fd, (sockaddr*)(&addr_obj), sizeof(sockaddr_in))<0)
        {   //Still in process
            if (errno==EINPROGRESS)
                connected = false;
            //Error connecting
            else
                throw SocketError(SocketError::Reason::CONNECT);
        }

        //Connected
        if (connected)
        {   //Updtae socket status
            data->status = Status::CONNECTED;
            data->remote_addr = addr;
            data->remote_port = port;
            //Trigger connect event
            this->trigger("connect");

            return Promise<void>::resolved();
        }
        //Not connected; waiting for reactor to resolve
        else
        {   data->status = Status::CONNECTING;

            return Promise<void>([=](PromiseCtx<void> ctx) mutable
            {   //Listen to connect event
                this->on("connect", [=]() mutable
                {   ctx.resolve();
                });
            });
        }
    }

    //Write data to socket
    Promise<void> Socket::write(std::string data)
    {   auto sock_data = this->data;
        std::string& buffer = sock_data->buffer;
        size_t offset = 0;
        bool completed = false;

        //Append data to end of the buffer
        sock_data->buffer += data;
        //Keep writing until finished or blocked
        while (true)
        {   unsigned int write_len = std::min<size_t>(N_BYTES_PER_ROUND, buffer.size()-offset);
            //Write data to socket
            int count = ::write(sock_data->fd, buffer.c_str()+offset, write_len);
            if (count==-1)
            {   //Write error
                if ((errno!=EAGAIN)&&(errno!=EWOULDBLOCK))
                    throw SocketError(SocketError::Reason::WRITE);
                else
                    break;
            }

            //Move offset pointer
            offset += count;
            //Check if all data in buffer is written to socket
            completed = offset>=buffer.size();
            if (completed)
                break;
        }

        //Update written bytes count
        sock_data->bytes_written += offset;
        //Update buffer
        buffer = buffer.substr(offset);

        //Completed
        if (completed)
            return Promise<void>::resolved();
        //Not completed; wait for reactor to resolve the promise
        else
        {   size_t write_target = sock_data->bytes_written+buffer.size();
            return Promise<void>([=](PromiseCtx<void> ctx)
            {   sock_data->write_promise_queue.push(PromiseQueueItem(write_target, ctx));
            });
        }
    }

    //Close connection
    void Socket::close()
    {   auto data = this->data;
        if ((data->status!=Status::CONNECTED)&&(data->status!=Status::HALF_CLOSED))
            return;

        //Close socket
        if ((data->fd>=0)&&(::close(data->fd)<0))
            throw SocketError(SocketError::Reason::CLOSE);

        //Open
        if (data->status==Status::CONNECTED)
        {   data->fd = -1;
            data->status = Status::HALF_CLOSED;
        }
        //Peer closed
        else if ((data->status==Status::HALF_CLOSED)&&(data->fd>=0))
        {   //Set status and trigger "close" event
            data->status = Status::CLOSED;
            this->trigger("close");
            //Unregister server socket from reactor
            reactor_unreg(data->fd);
        }
    }

    //Get socket status
    Socket::Status Socket::status()
    {   return this->data->status;
    }

    //Get size of buffer used
    size_t Socket::buffer_size()
    {   return this->data->buffer.size();
    }

    //Get bytes read
    size_t Socket::bytes_read()
    {   return this->data->bytes_read;
    }

    //Get bytes written
    size_t Socket::bytes_written()
    {   return this->data->bytes_written;
    }

    //Server socket constructor
    ServerSocket::ServerSocket() : data(std::make_shared<ServerSocketData>())
    {   //Create socket file descriptor
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd==-1)
            throw SocketError(SocketError::Reason::CREATE);
        this->data->fd = fd;

        //Make socket non-block
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags==-1)
            throw SocketError(SocketError::Reason::MAKE_NON_BLOCK);
        flags |= O_NONBLOCK;
        if (fcntl(fd, F_SETFL, flags)<0)
            throw SocketError(SocketError::Reason::MAKE_NON_BLOCK);

        //SO_REUSEADDR
        int enable = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int))<0)
            throw SocketError(SocketError::Reason::REUSEADDR);

        //Register socket to reactor
        this->reactor_register();
    }

    //Listen
    void ServerSocket::listen(in_addr_t addr, in_port_t port, int backlog)
    {   auto data = this->data;
        if (data->status!=Status::IDLE)
            return;

        //Make socket address object
        sockaddr_in addr_obj;
        addr_obj.sin_family = AF_INET;
        addr_obj.sin_addr.s_addr = addr;
        addr_obj.sin_port = port;
        //Bind socket to given address
        if (bind(data->fd, (sockaddr*)(&addr_obj), sizeof(sockaddr_in))<0)
            throw SocketError(SocketError::Reason::BIND);
        //Listen on given address
        if (::listen(data->fd, backlog)<0)
            throw SocketError(SocketError::Reason::LISTEN);
        //Set local address and port
        data->local_addr = addr;
        data->local_port = port;

        data->status = Status::LISTENING;
    }

    //Close connection
    void ServerSocket::close()
    {   auto data = this->data;

        //Unregister socket from reactor
        reactor_unreg(data->fd);
        //Close socket
        if (::close(data->fd)<0)
            throw SocketError(SocketError::Reason::CLOSE);
        //Set status and trigger "close" event
        data->status = Status::CLOSED;
        this->trigger("close");
    }

    //Get local address and port
    bool ServerSocket::local_addr(in_addr_t* addr, in_port_t* port)
    {   auto data = this->data;
        if (data->status!=Status::LISTENING)
            return false;

        *addr = data->local_addr;
        *port = data->local_port;
        return true;
    }

    //Get server socket status
    ServerSocket::Status ServerSocket::status()
    {   return this->data->status;
    }
}
