#include <libasync/event.h>
#include <libasync/misc.h>

namespace libasync
{   //Internal constructor
    EventMixin::EventMixin() : store(std::make_shared<EventMixin::EventHandlerStore>()) {}

    //Remove event listener
    bool EventMixin::off(std::string event, EventHandler handler)
    {   auto store = this->store;
        //Find event handler store
        auto result_ptr = store->find(event);
        bool store_found = result_ptr!=store->end();
        //Handler found flag
        bool handler_found = false;

        //Store not found
        if (!store_found)
            return false;
        //Find event handler
        uintptr_t handler_ptr = func_addr(handler);
        result_ptr->second.remove_if([&](EventHandler _handler)
        {   bool addr_equal = handler_ptr==func_addr(_handler);
            handler_found = handler_found||addr_equal;
            return addr_equal;
        });

        return handler_found;
    }
}
