# CHANGELOG — Aegis-X Protection Suite

All notable changes and refactoring steps for **Aegis-X Protection Suite** (by **Sahil**) are documented in this file.

This project is based on original open-source work from [CS2AC](https://github.com/karola3vax/CS2AC) and [CS2FOW](https://github.com/karola3vax/CS2FOW) created by **karola3vax**, licensed under **AGPL-3.0**.

---

## [2.0.0-aegisx] - 2026-07-28

### Added & Re-Architected
- **Client-Side Protection Executable (`AegisX_Guard.exe`)**: Real-time Windows launcher and background monitor loop.
- **Client-Side Fog-Of-War (`src/client_fow.cpp`)**: 3D spatial raycasting with client memory coordinate culling (`Vector(0,0,0)`).
- **Inline Hook & VMT Detector (`src/hook_detector.cpp`)**: Scans `cs2.exe` memory for `0xE9` inline detours, VMT hijacking, and unbacked `RWX` manual-mapped pages.
- **Transparent Overlay Scanner (`src/external_detector.cpp`)**: Scans desktop for `WS_EX_TOPMOST` + `WS_EX_TRANSPARENT` cheat overlays.
- **Anti-Debugging Shield (`src/anti_debug.cpp`)**: Hides threads via `NtSetInformationThread` and checks `DR0`-`DR3` hardware breakpoints.
- **PCIe DMA Hardware Card Scanner (`src/dma_shield.cpp`)**: Scans PCIe configuration spaces using `SetupAPI` for CaptainDMA, EnigmaDMA, Screamer, and Xilinx FPGA boards.
- **Hypervisor CPUID Timing Detector (`src/hypervisor_detector.cpp`)**: Measures `RDTSC` cycle latency across `CPUID` calls to flag VM-Exit cycle delays (> 800 cycles).
- **AI Computer Vision Capture Monitor (`src/ai_cv_detector.cpp`)**: Detects `IDXGIOutputDuplication` DirectX frame capture hooks used by YOLO / AI vision aimbots.
- **BYOVD Kernel Driver Shield (`src/kernel_guard.cpp`)**: Scans loaded system drivers for vulnerable drivers (`Capcom.sys`, `RTCore64.sys`, `gdrv.sys`).
- **Memory Code Section CRC32 Verifier (`src/memory_guard.cpp`)**: Computes periodic CRC32 checksums across `.text` sections in `client.dll` and `engine2.dll`.
- **In-Game Live Telemetry & Profile Engine (`src/leetify_analyzer.cpp`)**: Evaluates real-time reaction times (<100ms / 0ms triggerbot), 100 Aim Scores, and pre-aim efficiency with zero API keys required.

### Refactored & Renamed
- **Unified Namespaces**: Re-organized code under `vacguard::core`, `vacguard::detection`, `vacguard::fow`, and `vacguard::client`.
- **Detection Module Engine Aliases**:
  - `AimbotModule` → `VectorAimEngine`
  - `AimlockModule` → `TargetTrackingEngine`
  - `AntiAimModule` → `PitchYawAnomalyEngine`
  - `AutoStrafe` → `AirVelocityEngine`
  - `Bhop` → `JumpIntervalEngine`
  - `DLLInjection` → `EventSubscriptionGuard`
  - `Desubticking` → `SubtickTimestampVerifier`
  - `DoubletapModule` → `RapidFireEngine`
  - `Hyperscroll` → `ScrollInputBurstGuard`
  - `InhumanAccuracyModule` → `HitDistributionEngine`
  - `InvalidCVar` → `ClientCvarGuard`
  - `InvalidInput` → `CommandSequenceVerifier`
  - `IrregularBehaviorModule` → `AirborneNoScopeEngine`
  - `NamechangerModule` → `IdentitySpamGuard`
  - `Nulls` → `CounterStrafeKeyGuard`
  - `SilentAimModule` → `ImpactDisagreementEngine`
  - `SubtickSpam` → `SubtickBurstGuard`

### Code Quality & Readability
- Modernized C++20 numerics and constants (`std::numbers::pi`, `kMaxCommandStreamBuffer`, `kSnapWindowEvaluationTicks`).
- Added formal `THIRD_PARTY_NOTICES.md` documenting open-source AGPL-3.0 origins and credits to **karola3vax**.
