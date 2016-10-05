#pragma once

#include <string>
#include <functional>
#include <unordered_map>
#include <list>
#include <memory>
#include <type_traits>
#include <boost/any.hpp>
#include <libasync/func_traits.h>

namespace libasync
{   //Event mix-in class
    class EventMixin
    {public:
        //Event handler type
        typedef std::function<void(boost::any)> EventHandler;
    private:
        //Event handler store type
        typedef std::unordered_map<std::string, std::list<EventHandler>> EventHandlerStore;
        //Event handler store reference type
        typedef std::shared_ptr<EventHandlerStore> EventHandlerStoreRef;

        //Event handler store
        EventHandlerStoreRef store;
    protected:
        //Internal constructor
        EventMixin();

        //Trigger event
        template <typename T>
        void trigger(std::string event, T result)
        {   auto store = this->store;
            //Find event handler store
            auto result_ptr = store->find(event);
            bool found = result_ptr!=store->end();

            //Found
            if (found)
                for (auto handler : result_ptr->second)
                    handler(result);
        }

        void trigger(std::string event);
    public:
        //Add event listener
        template <typename HT>
        EventHandler on(std::string event, HT handler)
        {   //Check function signature
            static_assert(
                std::is_same<void, typename FnTrait<HT>::ReturnType>::value,
                "The event handler must not return anything."
            );
            static_assert(
                FnTrait<HT>::n_args==1,
                "The event handler must have exactly one argument."
            );
            typedef typename FnTrait<HT>::template Arg<0>::Type ResultType;

            auto store = this->store;
            //Event handler
            EventHandler _handler = [&](boost::any _result)
            {   handler(detail::cast_any<HT>(_result));
            };

            //Find event handler store for specific event
            auto result_ptr = store->find(event);
            bool found = result_ptr!=store->end();
            //Found
            if (found)
                result_ptr->second.push_back(_handler);
            //Not found
            else
            {   std::list<EventHandler> event_store;
                event_store.push_back(_handler);
                (*store)[event] = event_store;
            }

            return handler;
        }

        //Remove event listener
        bool off(std::string event, EventHandler handler);
    };
}
