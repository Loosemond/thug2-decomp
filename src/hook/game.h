#pragma once

#include <cstdint>

#if defined(THUG2_STANDALONE) && defined(THUG2_WEB)
// NOT ORIGINAL (the web port, port/): the exe's .rdata / .data are loaded at their own addresses and each REPLACE line
// maps the original's address to its function (port/src/hook/web_hook.cpp: every indirect call looks its target up
// there first), so a variable is read where it is and a call by address - or through the exe's vtables and tables -
// lands in our function, as in the hooked build.
template <typename T>
T& GameVariable(uintptr_t address)
{
    return *reinterpret_cast<T*>(address);
}
// (The address goes through a variable that's always 0: a call to a constant table slot past the table's size would
// be folded into a trap by the wasm optimizer - it can't know the lookup comes first.)
//
// A call to an address with no source (library code: GameSpy, the C runtime, ...) gets a stand-in there on first use,
// of the call's own type, that does nothing and returns zero - logged once (port/src/hook/web_hook.cpp).
extern "C" uintptr_t thug2_code_offset;
extern "C" uint32_t thug2_slots_filled[];
extern "C" void thug2_place_stub(uintptr_t address, void* stub, const char* type);

template <typename Fn>
struct GameFunctionStub;
template <typename R, typename... A>
struct GameFunctionStub<R (*)(A...)> {
    static R Call(A...)
    {
        if constexpr (__is_same(R, void))
            return;
        else if constexpr (__is_reference(R)) {
            static __remove_reference_t(R) none{};
            return none;
        } else
            return R();
    }
};
template <typename R, typename... A>
struct GameFunctionStub<R (*)(A..., ...)> : GameFunctionStub<R (*)(A...)> {
};

template <typename Fn>
Fn GameFunction(uintptr_t address)
{
    if (__builtin_expect(!((thug2_slots_filled[address >> 5] >> (address & 31)) & 1), 0))
        thug2_place_stub(address, reinterpret_cast<void*>(&GameFunctionStub<Fn>::Call), __PRETTY_FUNCTION__);
    return reinterpret_cast<Fn>(address + thug2_code_offset);
}
#elif defined(THUG2_STANDALONE)
// The standalone build (no original exe): declared, never defined - every use left is an unresolved symbol naming the
// function that uses it (tools/standalone_report.ts lists them). Each one has to become a real variable / a call.
template <typename T>
T& GameVariable(uintptr_t address);
template <typename Fn>
Fn GameFunction(uintptr_t address);
#else
// A variable that lives in the exe's data section. Until every function that uses it has been rewritten,
// the original code and ours must share the same storage, so we refer to it by address:
//
//   static auto& s_nextId = GameVariable<uint32_t>(0x00690FA0);
template <typename T>
T& GameVariable(uintptr_t address)
{
    return *reinterpret_cast<T*>(address);
}

// A function in the exe that hasn't been rewritten yet, called by address:
//
//   auto allocate = GameFunction<void*(__thiscall*)(Manager*, unsigned, int, Heap*)>(0x005BAFC0);
//
// Once that function is rewritten, call our version directly instead (calling a replaced function by
// address would just land in our code anyway).
template <typename Fn>
Fn GameFunction(uintptr_t address)
{
    return reinterpret_cast<Fn>(address);
}
#endif
