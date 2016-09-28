#pragma once

namespace libasync
{   //Choose type from list by index
    template <size_t n, typename... List>
    struct ChooseType;

    template <size_t n, typename Head, typename... Tail>
    struct ChooseType<n, Head, Tail...> : public ChooseType<n-1, Tail...> {};

    template <typename Head, typename... Tail>
    struct ChooseType<0, Head, Tail...>
    {   typedef Head Type;
    };

    //Function trait definition
    template <typename T>
    struct FnTrait;

    //Functor trait
    template <typename T>
    struct __FunctorTrait;

    template <typename RT, typename CT, typename... AT>
    struct __FunctorTrait<RT(CT::*)(AT...)> : public FnTrait<RT(AT...)> {};

    template <typename RT, typename CT, typename... AT>
    struct __FunctorTrait<RT(CT::*)(AT...) const> : public FnTrait<RT(AT...)> {};

    template <typename RT, typename CT, typename... AT>
    struct __FunctorTrait<RT(CT::*)(AT...) volatile> : public FnTrait<RT(AT...)> {};

    template <typename RT, typename CT, typename... AT>
    struct __FunctorTrait<RT(CT::*)(AT...) const volatile> : public FnTrait<RT(AT...)> {};

    //Functor
    template <typename T>
    struct FnTrait : public __FunctorTrait<decltype(&T::operator())> {};

    //Function
    template <typename RT, typename... AT>
    struct FnTrait<RT(AT...)>
    {   //Return type
        typedef RT ReturnType;
        //Argument type
        template <size_t n>
        using Arg = ChooseType<n, AT...>;

        //Arguments amount
        static constexpr size_t n_args = sizeof...(AT);
    };

    //Function pointer
    template <typename RT, typename... AT>
    struct FnTrait<RT(*)(AT...)> : public FnTrait<RT(AT...)> {};

    //Member function
    template <typename RT, typename CT, typename... AT>
    struct FnTrait<RT(CT::*)(AT...)> : public FnTrait<RT(CT&, AT...)> {};

    template <typename RT, typename CT, typename... AT>
    struct FnTrait<RT(CT::*)(AT...) const> : public FnTrait<RT(CT&, AT...)> {};

    template <typename RT, typename CT, typename... AT>
    struct FnTrait<RT(CT::*)(AT...) volatile> : public FnTrait<RT(CT&, AT...)> {};

    template <typename RT, typename CT, typename... AT>
    struct FnTrait<RT(CT::*)(AT...) const volatile> : public FnTrait<RT(CT&, AT...)> {};
}
