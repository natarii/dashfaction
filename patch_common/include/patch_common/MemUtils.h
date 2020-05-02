#pragma once

#include <cstring>
#include <stdint.h>

template<typename T>
struct TypeIdentity
{
    using type = T;
};

void WriteMem(unsigned addr, const void* data, unsigned size);
void UnprotectMem(void* ptr, unsigned len);

template<typename T>
void WriteMem(uintptr_t addr, typename TypeIdentity<T>::type value)
{
    WriteMem(addr, &value, sizeof(value));
}

inline void WriteMem(unsigned addr, const void* data, unsigned size, unsigned num_repeat)
{
    while (num_repeat > 0) {
        WriteMem(addr, data, size);
        addr += size;
        --num_repeat;
    }
}

template<typename T>
inline void WriteMemPtr(unsigned addr, T* value)
{
    WriteMem(addr, &value, sizeof(value));
}

inline void WriteMemStr(unsigned addr, const char* str)
{
    WriteMem(addr, str, std::strlen(str) + 1);
}

template<typename T>
constexpr T& AddrAsRef(uintptr_t addr)
{
    return *reinterpret_cast<T*>(addr);
}

template<typename T>
T& StructFieldRef(void* struct_ptr, size_t offset)
{
    auto addr = reinterpret_cast<uintptr_t>(struct_ptr) + offset;
    return *reinterpret_cast<T*>(addr);
}

// Note: references will not be properly passed as argments unless type is specified explicity.
// The reason for that is that argument deduction strips reference from type.
class AddrCaller
{
    uintptr_t addr_;

public:
    constexpr AddrCaller(uintptr_t addr) : addr_(addr)
    {}

    template<typename RetVal = void, typename... A>
    constexpr RetVal c_call(A... args)
    {
        return AddrAsRef<RetVal __cdecl(A...)>(addr_)(args...);
    }

    template<typename RetVal = void, typename... A>
    constexpr RetVal this_call(A... args)
    {
        return reinterpret_cast<RetVal(__thiscall*)(A...)>(addr_)(args...);
    }

    template<typename RetVal = void, typename... A>
    constexpr RetVal fast_call(A... args)
    {
        return AddrAsRef<RetVal __fastcall(A...)>(addr_)(args...);
    }

    template<typename RetVal = void, typename... A>
    constexpr RetVal std_call(A... args)
    {
        return AddrAsRef<RetVal __stdcall(A...)>(addr_)(args...);
    }
};
