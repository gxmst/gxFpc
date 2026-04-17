# FragPunk External Observation Tool (External Inspector)

This project is a diagnostic and observation tool designed for the Unreal Engine 4 game *FragPunk*. It documents a comprehensive technical audit and research journey against modern kernel-level anti-cheat protections (NEAC/Phanuel).

## 🛡️ Technical Post-Mortem: Challenges & Pitfalls

The primary objective was to establish a stealthy, non-invasive memory-reading channel to locate the `UWorld` pointer without loading custom drivers or performing risky injections. Here are the core findings and blockers encountered during development:

### 1. Target Identification: The 9GB Hidden Giant
*   **The Pitfall**: The anti-cheat uses a "Shadow Process" strategy, presenting a 30MB launcher to mislead standard scanners.
*   **The Breakthrough**: By utilizing `NtQuerySystemInformation` to audit the system-wide process memory footprint, we successfully bypassed process-list obfuscation and identified the real engine process: **PID 32352**, consuming **9.7 GB** of RAM.

### 2. Core Blocker: Kernel-Level Object Callbacks (ObRegisterCallbacks)
*   **The Pitfall**: Despite running with local Administrator privileges, any `OpenProcess` request for `PROCESS_VM_READ` (0x10) access returns **Error 5 (Access Denied)**.
*   **The Diagnosis**: NEAC's kernel-mode driver registers object callbacks that dynamically strip high-privilege access from any handle opened to the game engine from User Mode (Ring 3).

### 3. Exhausted Stealth Bypass Attempts
To maintain system integrity and avoid cross-game bans, we performed the following non-invasive probes:
-   **Native Driver Scavenging**: Scanned for pre-installed, signed "vulnerable" drivers (e.g., from MSI, Asrock, ASUS) to use as a physical memory-reading bridge. Result: System environment was found to be clean of exploitable targets.
-   **Global Handle Auditing (Handle Hijacking)**: Attempted to sniff and duplicate existing high-privilege handles held by trusted system entities like Steam or the game's own launcher. Result: NEAC successfully revokes or isolates all valid handles post-initialization.
-   **User-Mode Privilege Escalation**: Tested `SeDebugPrivilege` and other standard Win32 permission elevations; all were successfully mitigated by the kernel-level callback hook.

## 📜 Current Status & Conclusion
**Pure external memory reading is currently impossible for this target without a kernel-level driver bypass or specialized memory-mapping hardware.**

## 💡 Recommended Future Paths
For those continuing research, this project has verified three remaining viable routes:
1.  **Option A (Internal Migration)**: Pivot to the `dllmain.cpp` project found in the root directory. Internal memory access bypasses the cross-process handle block entirely.
2.  **Option B (Kernel BYOVD)**: Utilize a manual mapper (e.g., `kdmapper`) to load a vulnerable signed driver for physical memory mapping.
3.  **Option C (VHD Isolation)**: Perform development in an isolated VHD dual-boot environment to prevent anti-cheat fingerprinting from affecting the main host OS.

---
> *Disclaimer: This project is for educational and security research purposes only.*
