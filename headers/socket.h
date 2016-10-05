#include <netinet/in.h>
#include <memory>
#include <exception>
#include <string>
#include <queue>
#include <libasync/promise.h>
#include <libasync/event.h>

namespace libasync
{   //Socket class
    class Socket : public EventMixin
    {public:
        //Socket status
        enum class Status
        {   IDLE,
            CONNECTING,
            CONNECTED,
            CLOSED
        };
    private:
        //Promise queue item
        struct PromiseQueueItem
        {   //Write target
            size_t target;
            //Promise context
            PromiseCtx<void> ctx;

            //Constructor
            PromiseQueueItem(size_t _target, PromiseCtx<void> _ctx) : target(_target), ctx(_ctx) {}
        };

        //Socket data type
        struct SocketData
        {   //Socket file descriptor
            int fd;
            //Status
            Status status;

            //Data buffer (Used for writing data)
            std::string buffer;
            //Bytes read
            size_t bytes_read;
            //Bytes written
            size_t bytes_written;

            //Write promise queue
            std::queue<PromiseQueueItem> write_promise_queue;
            //Allow write flag
            bool write_flag;

            //Local address
            in_addr_t local_addr;
            //Local port
            in_port_t local_port;
            //Remote address
            in_addr_t remote_addr;
            //Remote port
            in_port_t remote_port;

            //Constructor
            SocketData() : status(Status::IDLE), bytes_read(0), bytes_written(0), write_flag(true), local_addr(INADDR_NONE) {}
        };

        //Socket data reference type
        typedef std::shared_ptr<SocketData> SocketDataRef;

        //Socket data
        SocketDataRef data;

        //Internal constructor
        Socket(int fd);

        //Reactor task
        static void reactor_task();

        //Shared socket initialization logic
        void create();
        //Register socket to reactor
        void _register();

        //Friend classes
        friend class ServerSocket;
    public:
        //Constructor
        Socket();

        //Initialize socket module
        static void init();

        //Get local address and port
        bool local_addr(in_addr_t* addr, in_port_t* port);
        //Get remote address and port
        bool remote_addr(in_addr_t* addr, in_port_t* port);

        //Bind to given address and port
        void bind(in_addr_t addr, in_port_t port);
        //Connect to given address and port
        Promise<void> connect(in_addr_t addr, in_port_t port);
        //Write data to socket
        Promise<void> write(std::string data);
        //Close connection
        void close();

        //Get socket status
        Status status();

        //Get size of buffer used
        size_t buffer_size();
        //Get bytes read
        size_t bytes_read();
        //Get bytes written
        size_t bytes_written();
    };

    //Server socket class
    class ServerSocket : public EventMixin
    {public:
        //Socket status
        enum class Status
        {   IDLE,
            LISTENING,
            CLOSED
        };
    private:
        //Server socket data type
        struct ServerSocketData
        {   //Socket file descriptor
            int fd;
            //Status
            Status status;

            //Local address
            in_addr_t local_addr;
            //Local port
            in_addr_t local_port;

            //Constructor
            ServerSocketData() : status(Status::IDLE) {}
        };

        //Server socket data reference type
        typedef std::shared_ptr<ServerSocketData> ServerSocketDataRef;

        //Server socket data
        ServerSocketDataRef data;

        //Register socket to reactor
        void _register();

        //Friend classes
        friend class Socket;
    public:
        //Constructor
        ServerSocket();

        //Listen
        void listen(in_addr_t addr, in_port_t port, int backlog = SOMAXCONN);
        //Close connection
        void close();

        //Get local address and port
        bool local_addr(in_addr_t* addr, in_port_t* port);
        //Get socket status
        Status status();
    };

    //Socket exception
    class SocketError : public std::exception
    {public:
        //Reason
        enum class Reason
        {   REACTOR_INIT,
            REACTOR_QUERY,
            REACTOR_REG,
            CREATE,
            MAKE_NON_BLOCK,
            BIND,
            LISTEN,
            CONNECT,
            ACCEPT,
            READ,
            WRITE,
            GET_LOCAL_ADDR,
            CLOSE
        };

        //Get reason
        Reason reason() const noexcept;
        //Get error number
        int error_num() const noexcept;
        //Get error information
        const char* what() const noexcept;
    private:
        //Reason
        Reason _reason;
        //Error number
        int _error_num;

        //Internal constructor
        SocketError(Reason __reason, int __error_num = errno);

        //Friend classes
        friend class Socket;
        friend class ServerSocket;
    };
}
