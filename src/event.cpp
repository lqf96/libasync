#include <libasync/event.h>
#include <libasync/misc.h>

namespace libasync
{   //Internal constructor
    EventMixin::EventMixin() : data(std::make_shared<EventMixinData>()) {}

    //Trigger event
    void EventMixin::trigger(std::string event)
    {   return this->trigger(event, boost::any());
    }

    //Remove event listener
    bool EventMixin::off(std::string event, size_t handle)
    {   auto store = this->data->store;

        //Find event handler store
        auto result_ptr = store.find(event);
        if (result_ptr==store.end())
            return false;
        //Find event handler
        EventHandlerStore& event_store = store[event];
        auto handler_ptr = event_store.find(handle);
        if (handler_ptr==event_store.end())
            return false;
        //Remove handler
        event_store.erase(handler_ptr);

        return true;
    }
}
