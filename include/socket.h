#include <netinet/in.h>
#include <memory>
#include <libasync/promise.h>
#include <libasync/event.h>

namespace libasync
{   //Socket class
    class Socket
    {public:
        //Socket status
        enum Status
        {

        };
    private:
        //Socket data type
        struct SocketData
        {

        };

        //Socket data reference type
        typedef std::shared_ptr<SocketData> SocketDataRef;

        //Socket data
        SocketDataRef data;
    public:
        //Constructor
        Socket(in_addr_t addr, in_port_t port = 0);

        //Get local address and port
        bool addr(in_addr_t* addr, in_port_t* port);
        //Get remote address and port
        bool remote_addr(in_addr_t* addr, in_port_t* port);

        //Pause reading data
        void pause();
        //Resume reading data
        void resume();

        //Connect to given address and port
        Promise<void> connect(in_addr_t addr, in_port_t port);
        //Write data to socket
        Promise<void> write();

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
    class ServerSocket
    {private:
        //Server socket data type
        struct ServerSocketData
        {   //Socket file descriptor
            int fd;
        };

        //Server socket data reference type
        typedef std::shared_ptr<ServerSocketData> ServerSocketDataRef;

        //Server socket data
        ServerSocketDataRef data;
    public:
        //Constructor
        ServerSocket();
        //Initialize server socket
        Promise<void> init();

        //Listen
        Promise<void> listen(in_addr_t addr, in_port_t port, int backlog);
        //Close connection
        Promise<void> close();

        //Server is listening or not
        bool listening(in_addr_t* addr, in_port_t* port);

        //Get connection mount
        size_t n_conn();
        //Get and set max connections
        size_t max_conn(size_t new_max_conn = 0);
    };
}
