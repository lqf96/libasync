#pragma once

#include <functional>
#include <type_traits>
#include <libasync/func_traits.h>

namespace libasync
{   //Get target address of function wrapper
    template <typename FnSig>
    uintptr_t func_addr(std::function<FnSig> func)
    {   return reinterpret_cast<uintptr_t>(func.template target<void>());
    }
}
