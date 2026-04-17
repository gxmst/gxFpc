#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include <tlhelp32.h>
#include <iostream>
#include <algorithm>
#include <map>
#include <psapi.h>

#pragma comment(lib, "psapi.lib")

class Memory {
public:
    DWORD processId = 0;
    HANDLE processHandle = nullptr;
    uintptr_t baseAddress = 0;
    uintptr_t baseSize = 0;
    std::map<uintptr_t, uintptr_t> knownModules;

    ~Memory() {
        if (processHandle) CloseHandle(processHandle);
    }

    // V7: Deep Discovery - Scans ALL PIDs for the 1GB+ RAM fingerprint
    bool Init(const char* processName) {
        std::cout << "[*] Starting Deep PID Audit (V7)..." << std::endl;
        processId = ScanForGameProcess(processName);
        
        if (!processId) {
            std::cerr << "[-] Error: Game engine fingerprint not found in system." << std::endl;
            return false;
        }

        processHandle = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
        if (!processHandle) {
            std::cerr << "[-] Error: Found PID " << processId << " but Access Denied (E_ACCESSDENIED)." << std::endl;
            return false;
        }

        UpdateModuleInfo(processId, processName);
        AuditModules();
        return true;
    }

    template <typename T>
    T Read(uintptr_t address) {
        T buffer;
        memset(&buffer, 0, sizeof(T));
        ReadProcessMemory(processHandle, (LPCVOID)address, &buffer, sizeof(T), nullptr);
        return buffer;
    }

    template <typename T>
    bool Read(uintptr_t address, T& buffer) {
        return ReadProcessMemory(processHandle, (LPCVOID)address, &buffer, sizeof(T), nullptr);
    }

    bool ReadRaw(uintptr_t address, void* buffer, size_t size) {
        return ReadProcessMemory(processHandle, (LPCVOID)address, buffer, size, nullptr);
    }

    uintptr_t GetRIPRelative(uintptr_t address, int offset, int instructionSize) {
        int rel = 0;
        if (Read(address + offset, rel)) {
            return address + instructionSize + rel;
        }
        return 0;
    }

    uintptr_t PatternScan(const char* pattern, const char* mask) {
        size_t patternLen = strlen(mask);
        MEMORY_BASIC_INFORMATION mbi;
        uintptr_t currentAddr = 0x0;
        uintptr_t maxAddr = 0x7FFFFFFFFFFF;

        while (currentAddr < maxAddr && VirtualQueryEx(processHandle, (LPCVOID)currentAddr, &mbi, sizeof(mbi))) {
            uintptr_t regionStart = (uintptr_t)mbi.BaseAddress;
            uintptr_t regionEnd = regionStart + mbi.RegionSize;

            bool isSystemModule = false;
            for (auto const& [start, end] : knownModules) {
                if (regionStart >= start && regionStart < end) {
                    isSystemModule = true;
                    break;
                }
            }

            if (isSystemModule) {
                currentAddr = regionEnd;
                continue;
            }

            bool isExecutable = (mbi.Protect & (PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE));
            if (mbi.State == MEM_COMMIT && isExecutable) {
                const size_t chunkSize = 4096 * 512;
                std::vector<uint8_t> buffer(chunkSize);
                
                for (uintptr_t i = regionStart; i < regionEnd; i += (chunkSize - patternLen)) {
                    size_t bytesToRead = (std::min)(chunkSize, regionEnd - i);
                    if (!ReadRaw(i, buffer.data(), bytesToRead)) continue;

                    for (size_t j = 0; j < (bytesToRead - patternLen); j++) {
                        bool found = true;
                        for (size_t k = 0; k < patternLen; k++) {
                            if (mask[k] != '?' && (uint8_t)pattern[k] != buffer[j + k]) {
                                found = false;
                                break;
                            }
                        }
                        if (found) return i + j;
                    }
                }
            }
            currentAddr = regionEnd;
        }
        return 0;
    }

    void AuditModules() {
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);
        if (snapshot != INVALID_HANDLE_VALUE) {
            MODULEENTRY32 moduleEntry;
            moduleEntry.dwSize = sizeof(moduleEntry);
            if (Module32First(snapshot, &moduleEntry)) {
                knownModules.clear();
                do {
                    knownModules[(uintptr_t)moduleEntry.modBaseAddr] = (uintptr_t)moduleEntry.modBaseAddr + moduleEntry.modBaseSize;
                } while (Module32Next(snapshot, &moduleEntry));
            }
            CloseHandle(snapshot);
        }
    }

private:
    // V7: Brute force PID sweeper combined with RAM fingerprinting
    DWORD ScanForGameProcess(const char* targetName) {
        DWORD aProcesses[1024], cbNeeded, cProcesses;
        if (!EnumProcesses(aProcesses, sizeof(aProcesses), &cbNeeded)) return 0;
        cProcesses = cbNeeded / sizeof(DWORD);

        DWORD bestPid = 0;
        size_t maxRam = 0;

        for (unsigned int i = 0; i < cProcesses; i++) {
            if (aProcesses[i] == 0) continue;

            HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, aProcesses[i]);
            if (hProcess) {
                char szProcessName[MAX_PATH] = "<unknown>";
                GetModuleBaseNameA(hProcess, NULL, szProcessName, sizeof(szProcessName));

                PROCESS_MEMORY_COUNTERS pmc;
                if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
                    size_t currentRam = pmc.WorkingSetSize;
                    
                    // IF it's the target name OR it's a huge anonymous process (> 2GB)
                    bool isTarget = (_stricmp(szProcessName, targetName) == 0);
                    bool isGigantic = (currentRam > (1024ULL * 1024 * 1024)); // > 1GB

                    if (isGigantic) {
                        std::printf("[Sweep] Found Large Process: %s (PID: %lu) | RAM: %llu MB\n", 
                            szProcessName, aProcesses[i], (unsigned long long)(currentRam / 1024 / 1024));
                        
                        // We prioritize the largest one because the real game engine is always the heaviest
                        if (currentRam > maxRam) {
                            maxRam = currentRam;
                            bestPid = aProcesses[i];
                        }
                    }
                }
                CloseHandle(hProcess);
            }
        }
        return bestPid;
    }

    void UpdateModuleInfo(DWORD pid, const char* moduleName) {
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        if (snapshot != INVALID_HANDLE_VALUE) {
            MODULEENTRY32 moduleEntry;
            moduleEntry.dwSize = sizeof(moduleEntry);
            if (Module32First(snapshot, &moduleEntry)) {
                do {
                    if (!_stricmp(moduleEntry.szModule, moduleName)) {
                        baseAddress = (uintptr_t)moduleEntry.modBaseAddr;
                        baseSize = (uintptr_t)moduleEntry.modBaseSize;
                        break;
                    }
                } while (Module32Next(snapshot, &moduleEntry));
            }
            CloseHandle(snapshot);
        }
        // If still 0, we'll rely on SuperScan starting from 0x0
    }
};
