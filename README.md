# Libasync
Asynchronous C++ library with IO and multi-thread support.

## Modules & Features
* Promise (`libasync/promise.h`): Brings Promise/A+ promise from Javascript to C++.
* TaskLoop (`libasync/taskloop.h`): Simple and easy-to-use task loop API for executing permanent or oneshot tasks. Cooperates well with other task loops (Like GCD) or event loops.
* TCP Socket (`libasync/socket.h`): Asynchronous and efficient socket operations, powered by platform-specific event notification and/or asynchronous I/O APIs.
* Reactor (`libasync/reactor.h`): Responsible for polling event notification and I/O completion status from platform-specific APIs.
* Event Mix-in (`libasync/event.h`): Provide a handy event mix-in that turn a class into an event target.
* Generator (`libasync/generator.h`) (Unstable)
* Asynchronous function (`libasync/async_func.h`) (Unstable)

## API
* `libasync/promise.h`
  + `class PromiseCtx<T>`: Promise context type.
    - `.resolve(T)`: Resolve associated promise with value.
    - `.resolve(Promise<T>)`: Resolve associated promise with another promise.
    - `.reject<U>(U)`: Reject associated promise with exception.
  + `class Promise<T>`: Promise/A+ compliant promise type.
    - `Promise<T>((PromiseCtx<T>) -> void)`: Construct a promise with given executor.
    - `::resolved(T)`: Construct a promise with resolved state from given value.
    - `::resolved(Promise<T>)`: Construct a promise whose status is associated with given promise.
    - `::rejected<U>(U)`: Construct a promise with rejected state from given exception.
    - `.status()`: Get promise status.
    - `.then<U>((T) -> U)`: Promise `then` method. Executed when promise is resolved.
    - `.then<U>((T) -> Promise<U>)`: Promise `then` method. Executed when promise is resolved.
    - `.catch<E, U>((E) -> U)`: Promise `catch` method. Executed when promise is rejected.
    - `.catch<E, U>((E) -> Promise<U>)`: Promise `catch` method. Executed when promise is rejected.
  + `promise_init()`: Initialize promise module.
* `libasync/taskloop.h`
  + `class TaskLoop`: Task loop type.
    - `TaskLoop()`: Construct a new task loop.
    - `::thread_loop()`: Get root task for current thread.
    - `.add(() -> void)`: Add a permanent task to task loop.
    - `.oneshot(() -> void)`: Add a oneshot task to task loop.
    - `.n_permanent_tasks()`: Get amount of permanent tasks.
    - `.n_oneshot_tasks()`: Get amount of oneshot tasks.
    - `.run()`: Run loop forever.
    - `.run_once()`: Run loop once.
* `libasync/event.h`
  + `class EventMixin`: Event mix-in.
    - `.on<T>(string, (T) -> void)`: Add an event handler.
    - `.on<T>(string, () -> void)`: Add an event handler.
    - `.off(string, size_t)`: Remove an event handler by handle.
    - `.trigger<T>(string, T)`: Trigger an event.
    - `.trigger(string)`: Trigger an event.
* `libasync/socket.h`
  + `class Socket`: Socket type. (An event target)
    - `Socket()`: Construct a new socket.
    - `.local_addr(in_addr_t*, in_port_t*)`: Get local address and port.
    - `.remote_addr(in_addr_t*, in_port_t*)`: Get remote address and port.
    - `.bind(in_addr_t, in_port_t)`: Bind to given address and port.
    - `.connect(in_addr_t, in_port_t)`: Connect to given address and port.
    - `.write(string)`: Write data to socket.
    - `.close()`: Close connection.
    - `.status()`: Get socket status.
    - `.buffer_size()`: Get size of buffer used.
    - `.bytes_read()`: Get bytes read.
    - `.bytes_written()`: Get bytes written.
    - Event `connect`: Socket successfully connected to remote.
    - Event `error`: Socket error happened.
    - Event `end`: Remote closed connection.
    - Event `close`: Connection fully closed.
  + `class ServerSocket`: Server socket type. (An event target)
    - `.listen(in_addr_t, in_port_t, int)`: Listen for incoming connections.
    - `.close()`: Close server socket. Will not close connection already made.
    - `.local_addr(in_addr_t*, in_port_t*)`: Get local address and port.
    - `.status()`: Get socket status.
    - Event `connect`: Incoming connection received.
    - Event `close`: Server socket closed.
  + `class SocketError`: Socket exception.
    - `.reason()`: Get reason for the error.
    - `.error_num()`: Get error number returned from POSIX APIs.
    - `.what()`: Get error information string.
* `libasync/reactor.h`
  + `class ReactorError`: Reactor exception.
    - `.reason()`: Get reason for the error.
    - `.error_num()`: Get error number returned from platform-specific APIs.
    - `.what()`: Get error information string.
  + `reactor_init()`: Initialize reactor module.

## License
[MIT License](LICENSE)
