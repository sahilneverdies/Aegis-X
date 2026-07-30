<div align="center">

# 🛡️ Aegis-X — Next-Generation Pure Client-Side Anti-Cheat Suite for CS2

### Created & Developed by Sahil

**Counter-Strike 2 is at its best when every shot, clutch, and win is earned.**

Aegis-X operates as an unbypassable, pure client-side anti-cheat daemon (`AegisX_ClientGuard.exe`) running natively on player systems (FACEIT / ESEA style) with **100% zero server plugin dependencies**.

[Features](#-core-client-side-architecture) · [Building from Source](#-building-from-source) · [Remote Auto-Updater](#-remote-auto-updater-system) · [Credits](#-credits)

---

</div>

<div align="center">

<a href="https://github.com/sahilneverdies/Aegis-X">
<img src="docs/showcase/cs2fow-wallhack.gif" width="100%" alt="Aegis-X Client Memory Fog-Of-War Operating in Real-Time">
</a>

**Catch the cheat. Starve the wallhack.**

<sub>Aegis-X detects cheating behavior on the player's system while Client Fog-Of-War culls local memory coordinates, rendering external wallhacks (ESP), DMA hardware cards, and web radars 100% blind.</sub>

</div>

---

## ⚡ Core Client-Side Architecture (`AegisX_ClientGuard.exe`)

Aegis-X is built from the ground up in native C++20 for Windows x64 as a high-performance, low-overhead client security daemon.

### 🎨 1. Skeuomorphic Pitch-Black GUI Dashboard ([gui_window.cpp](file:///e:/cs2%20anticheats/CS2AC_FOW/src/gui_window.cpp))
- **Steam Auto-Authentication**: Reads active Steam credentials from Windows Registry (`AutoLoginUser`, `PersonaName`, `ActiveUser` / `SteamID64`) and loads high-resolution profile avatars (`_full.png` / `_medium.png`) via GDI+ Bicubic smoothing.
- **Pitch-Black Tactile Aesthetics**: Deep dark graphite styling (`#080A0E`) with 3D specular metallic bevels, tactile stitch borders, and ClearType font anti-aliasing.
- **3D Specular LED Indicators**: Real-time status pills (`[+] KERNEL GUARD`, `[+] PCIe DMA SHIELD`, `[+] HYPERVISOR GUARD`, `[+] ANTI-TAMPER`) with glowing neon green LED spheres.

### 🔒 2. Kernel & Process Self-Protection ([anti_tamper.cpp](file:///e:/cs2%20anticheats/CS2AC_FOW/src/anti_tamper.cpp))
- **DACL Handle Stripping**: Enforces restrictive Kernel Security Descriptors (DACLs) stripping `PROCESS_TERMINATE`, `PROCESS_VM_READ`, and `PROCESS_VM_WRITE` rights from task managers, cheat injectors, and debuggers.
- **Code Signing Policy**: Enforces `MicrosoftSignedOnly` process thread mitigations to block unauthorized DLL injection.

### ⚡ 3. PCIe DMA Hardware Card Shield ([dma_shield.cpp](file:///e:/cs2%20anticheats/CS2AC_FOW/src/dma_shield.cpp))
- **PCIe Bus Hardware Scanner**: Enumerates hardware device descriptors via Windows `SetupAPI` to detect hardware memory attack cards (CaptainDMA, EnigmaDMA, Screamer, Xilinx FPGA boards).

### 🌀 4. Hypervisor CPUID Timing Verifier ([hypervisor_detector.cpp](file:///e:/cs2%20anticheats/CS2AC_FOW/src/hypervisor_detector.cpp))
- **CPUID Latency Measurement**: Measures CPU cycle latency across `CPUID` execution using `RDTSC` / `RDTSCP` instructions to identify Type-1 & Type-2 hypervisors (VT-x, BluePill) forcing VM-Exits (`> 800 cycles`).

### 👁️ 5. AI Computer Vision Detector ([ai_cv_detector.cpp](file:///e:/cs2%20anticheats/CS2AC_FOW/src/ai_cv_detector.cpp))
- **DXGI Frame Capture Hook Scan**: Monitors DXGI Desktop Duplication API (`IDXGIOutputDuplication`) handles to detect YOLO / AI vision aimbot frame capture mechanisms.

### 🔍 6. Smart External Overlay Detector ([external_detector.cpp](file:///e:/cs2%20anticheats/CS2AC_FOW/src/external_detector.cpp))
- **Transparent Top-Most Window Scan**: Inspects `WS_EX_TOPMOST` and `WS_EX_TRANSPARENT` / `WS_EX_LAYERED` windows overlaying the game rectangle.
- **Intelligent Process Whitelisting**: Employs `QueryFullProcessImageNameA` verification to whitelist benign desktop apps (Visual Studio, VS Code, Antigravity IDE, WhatsApp, Chrome, Edge, Discord, Steam, NVIDIA, AMD).

### 🌌 7. Client Memory Fog-Of-War ([client_fow.cpp](file:///e:/cs2%20anticheats/CS2AC_FOW/src/client_fow.cpp))
- **Client Memory Coordinate Culling**: Zeroes out occluded enemy vector coordinates (`Vector(0,0,0)`) in client memory, keeping external ESP wallhacks, DMA cards, and web radars 100% blind until players emerge into line-of-sight.

### ⏱️ 8. Heartbeat Watchdog & Anti-Suspension ([watchdog.cpp](file:///e:/cs2%20anticheats/CS2AC_FOW/src/watchdog.cpp))
- **Tick Delta Monitor**: Runs an encrypted thread monitoring tick deltas (`GetTickCount64`). Immediately terminates `cs2.exe` if Aegis-X is frozen, paused, or suspended by a cheat debugger (`> 2000ms`).

---

## 📡 Remote Auto-Updater System ([updater.cpp](file:///e:/cs2%20anticheats/CS2AC_FOW/src/updater.cpp))

Aegis-X features a live Remote Auto-Updater:
1. **GitHub Manifest Check**: On launch, Aegis-X queries [version.json](version.json) on GitHub.
2. **In-App Update Prompt**: If a new release is detected (e.g. `v3.1.0`), Aegis-X displays a dedicated update pop-up with changelog details and an interactive `[ UPDATE NOW ]` button.
3. **Automatic Dashboard Transition**: Upon completing the 1-click update installation, Aegis-X saves `version.dat` locally and seamlessly transitions into the main black dashboard.

---

## ⚙️ Building from Source

### Prerequisites
- **OS**: Windows 10 / 11 (x64)
- **Compiler**: Visual Studio 2022 (MSVC) with the *Desktop development with C++* workload
- **Build System**: CMake 3.20 or newer

### Build Instructions

1. **Clone the repository**:
   ```sh
   git clone --recursive https://github.com/sahilneverdies/Aegis-X.git
   cd Aegis-X
   ```

2. **Configure & Build Release Executable**:
   ```powershell
   cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
   cmake --build build --config Release
   ```

3. **Output Location**:
   The built binary will be located at:
   `build\Release\AegisX_ClientGuard.exe`

---

## 📜 License & Credits

Designed and maintained by **[Sahil](https://github.com/sahilneverdies)**.

Detection logic and spatial raycasting algorithms built upon foundations by **[karola3vax](https://github.com/karola3vax)**.

- **[CS2AC Repository](https://github.com/karola3vax/CS2AC)**
- **[CS2FOW Repository](https://github.com/karola3vax/CS2FOW)**
