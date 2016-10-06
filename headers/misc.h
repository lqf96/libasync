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

    //Implementation details
    namespace detail
    {   //Make exception pointer
        template <typename T>
        inline std::exception_ptr eptr_make(T error)
        {   return std::make_exception_ptr(error);
        }

        template <>
        inline std::exception_ptr eptr_make(std::exception_ptr eptr)
        {   return eptr;
        }

        //Cast exception pointer to given type
        template <typename T>
        inline T eptr_cast(std::exception_ptr eptr)
        {   try
            {   std::rethrow_exception(eptr);
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

        template <>
        inline std::exception_ptr eptr_cast(std::exception_ptr eptr)
        {   return eptr;
        }

        //Any cast helper
        template <typename T>
        inline T cast_any(boost::any value)
        {   return boost::any_cast<T>(value);
        }

        template <>
        inline boost::any cast_any(boost::any value)
        {   return value;
        }
    }
}
