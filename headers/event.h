#pragma once

#include <string>
#include <functional>
#include <unordered_map>
#include <list>
#include <memory>
#include <type_traits>
#include <boost/any.hpp>
#include <libasync/func_traits.h>
#include <libasync/misc.h>

namespace libasync
{   //Implementation details
    namespace detail
    {   //Event argument cast trait
        template <typename FT, size_t n_args>
        struct EventArgCastTrait;

        template <typename FT>
        struct EventArgCastTrait<FT, 0>
        {   typedef boost::any Type;
        };

        template <typename FT>
        struct EventArgCastTrait<FT, 1>
        {   typedef typename FnTrait<FT>::template Arg<0>::Type Type;
        };
    }

    //Event mix-in class
    class EventMixin
    {public:
        //Event handler type
        typedef std::function<void(boost::any)> EventHandler;
    private:
        //Event handler store type
        typedef std::unordered_map<size_t, EventHandler> EventHandlerStore;
        //Complete handler store type
        typedef std::unordered_map<std::string, EventHandlerStore> CompleteHandlerStore;

        //Event mix-in data type
        struct EventMixinData
        {   //Complete handler store
            CompleteHandlerStore store;
            //Handle counter
            size_t counter;

            //Constructor
            EventMixinData() : counter(0) {}
        };

        //Event mix-in data reference type
        typedef std::shared_ptr<EventMixinData> EventMixinDataRef;

        //Event mix-in data
        EventMixinDataRef data;
    protected:
        //Internal constructor
        EventMixin();

        //Trigger event
        template <typename T>
        void trigger(std::string event, T result)
        {   auto store = this->data->store;
            //Find event handler store
            auto result_ptr = store.find(event);

            //Not found; do nothing
            if (result_ptr==store.end())
                return;
            //Call back
            for (auto item_pair : result_ptr->second)
                item_pair.second(result);
        }

        void trigger(std::string event);
    public:
        //Add event listener
        template <typename HT>
        size_t on(std::string event, HT handler)
        {   //Check function signature
            static_assert(
                std::is_same<void, typename FnTrait<HT>::ReturnType>::value,
                "The event handler must not return anything."
            );
            static_assert(
                FnTrait<HT>::n_args<=1,
                "The event handler must have at most one argument."
            );
            typedef typename detail::EventArgCastTrait<HT, FnTrait<HT>::n_args>::Type ResultType;

            auto data = this->data;
            //Event handler
            EventHandler _handler = [=](boost::any _result)
            {   detail::CallWithOptArg<ResultType, HT>::call(handler, detail::cast_any<ResultType>(_result));
            };
            //Create a new handle
            size_t handle = data->counter;
            data->counter++;
            //Insert into handler store
            data->store[event][handle] = _handler;

            return handle;
        }

        //Remove event listener
        bool off(std::string event, size_t handle);
    };
}
