#include "anti_debug.h"
#include <winternl.h>

typedef NTSTATUS(NTAPI* pfnNtSetInformationThread)(
    HANDLE ThreadHandle,
    ULONG ThreadInformationClass,
    PVOID ThreadInformation,
    ULONG ThreadInformationLength
);

namespace cs2ac {

bool AntiDebug::HideCurrentThread() {
    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    if (!hNtdll) return false;

    auto NtSetInformationThread = reinterpret_cast<pfnNtSetInformationThread>(
        GetProcAddress(hNtdll, "NtSetInformationThread")
    );

    if (!NtSetInformationThread) return false;

    NTSTATUS status = NtSetInformationThread(GetCurrentThread(), 0x11, nullptr, 0);
    return NT_SUCCESS(status);
}

bool AntiDebug::IsDebuggerPresentCheck() {
    if (IsDebuggerPresent()) return true;

    BOOL isRemotePresent = FALSE;
    if (CheckRemoteDebuggerPresent(GetCurrentProcess(), &isRemotePresent) && isRemotePresent) {
        return true;
    }

    return false;
}

bool AntiDebug::CheckHardwareBreakpoints() {
    CONTEXT ctx{};
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    HANDLE hThread = GetCurrentThread();

    if (GetThreadContext(hThread, &ctx)) {
        if (ctx.Dr0 != 0 || ctx.Dr1 != 0 || ctx.Dr2 != 0 || ctx.Dr3 != 0) {
            return true;
        }
    }
    return false;
}

} // namespace cs2ac
