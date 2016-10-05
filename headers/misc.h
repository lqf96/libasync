#pragma once

#include <functional>
#include <type_traits>
#include <exception>
#include <boost/any.hpp>
#include <libasync/func_traits.h>

namespace libasync
{   //Get target address of function wrapper
    template <typename FnSig>
    uintptr_t func_addr(std::function<FnSig> func)
    {   return reinterpret_cast<uintptr_t>(func.template target<void>());
    }

    //Exception pointer cast
    template <typename T>
    T exception_ptr_cast(std::exception_ptr _e)
    {   try
        {   std::rethrow_exception(_e);
        }
        //Casted to type T
        catch (T e)
        {   return e;
        }
        //Type mismatch; bad cast
        catch (...)
        {   throw std::bad_cast();
        }
    }

#ifdef _LIBASYNC_SRC
    //Exception helper
    template <typename T>
    struct ErrorHelper
    {   //Make error
        static std::exception_ptr make(T error)
        {   return std::make_exception_ptr(error);
        }

        //Cast to given type
        static T cast(std::exception_ptr e)
        {   return exception_ptr_cast<T>(e);
        }
    };

    template <>
    struct ErrorHelper<std::exception_ptr>
    {   static std::exception_ptr make(std::exception_ptr e)
        {   return e;
        }

        static std::exception_ptr cast(std::exception_ptr e)
        {   return e;
        }
    };

    //Implementation details
    namespace detail
    {   //Any cast helper
        template <typename T>
        T cast_any(boost::any value)
        {   return boost::any_cast<T>(value);
        }

        //Any cast helper (No cast)
        boost::any cast_any(boost::any value)
        {   return value;
        }
    }
#endif
}
