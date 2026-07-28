#pragma once

#include <windows.h>

namespace cs2ac {

class AntiDebug {
public:
    static bool HideCurrentThread();
    static bool IsDebuggerPresentCheck();
    static bool CheckHardwareBreakpoints();
};

} // namespace cs2ac
