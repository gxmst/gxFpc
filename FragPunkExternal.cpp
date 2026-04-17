#include <iostream>
#include <vector>
#include <filesystem>
#include <string>
#include <windows.h>
#include "Memory.hpp"
#include "UE4.hpp"

namespace fs = std::filesystem;

// --- SIMPLE XOR STRING OBFUSCATION ---
template <size_t N>
class XorStr {
public:
    char data[N];
    static constexpr char key = 0x5A;

    constexpr XorStr(const char* str) {
        for (size_t i = 0; i < N; ++i) {
            data[i] = str[i] ^ key;
        }
    }

    std::string decrypt() const {
        std::string s = "";
        for (size_t i = 0; i < N - 1; ++i) {
            s += (data[i] ^ key);
        }
        return s;
    }
};

#define XOR(s) XorStr<sizeof(s)>(s).decrypt().c_str()

void CleanupTraces();

struct Vector3 {
    float x, y, z;
};

class Manager {
public:
    Memory mem;
    uintptr_t cachedRootComponent = 0;

    Manager() {
        SetConsoleTitleA(XOR("svchost")); 

        std::string target = XorStr<13>("FragPunk.exe").decrypt();
        std::cout << "[+] FragPunk External Inspector Starting..." << std::endl;
        
        std::cout << "[*] Searching for " << target << "..." << std::endl;
        while (!mem.Init(target.c_str())) {
            Sleep(1000);
        }

        std::cout << "[+] Successfully Hooked High-Memory Game Engine (PID: " << mem.processId << ")" << std::endl;
        if (mem.baseAddress != 0) {
            std::printf("[+] Entry Point: 0x%llX | Header Size: 0x%llX\n", (unsigned long long)mem.baseAddress, (unsigned long long)mem.baseSize);
        } else {
            std::cout << "[!] Note: Main module list hidden by NEAC. Continuing with Global SuperScan..." << std::endl;
        }
    }

    bool ResolvePlayerChain(UE4Handler& ue) {
        uintptr_t uWorld = ue.GetUWorld();
        if (!uWorld) return false;

        uintptr_t gameInstanceEnc = 0;
        if (!mem.Read(uWorld + offsets::OwningGameInstance, gameInstanceEnc)) return false;
        
        uintptr_t gameInstance = ue.Decrypt(gameInstanceEnc, 0x94F7BF7F, 16);
        if (!gameInstance) return false;

        uintptr_t localPlayersPtr = 0;
        if (!mem.Read(gameInstance + offsets::LocalPlayers, localPlayersPtr)) return false;
        
        uintptr_t localPlayer = 0;
        if (!mem.Read(localPlayersPtr, localPlayer)) return false;
        if (!localPlayer) return false;

        uintptr_t pcEnc = 0;
        if (!mem.Read(localPlayer + offsets::PlayerController, pcEnc)) return false;
        uintptr_t pc = ue.Decrypt(pcEnc, 0x9476F7EF, 16);
        if (!pc) return false;

        uintptr_t pawn = 0;
        if (!mem.Read(pc + offsets::AcknowledgedPawn, pawn)) return false;
        if (!pawn) return false;

        uintptr_t rootEnc = 0;
        if (!mem.Read(pawn + offsets::RootComponent, rootEnc)) return false;
        cachedRootComponent = ue.Decrypt(rootEnc, 0x947E77EB, 0x14);

        return cachedRootComponent != 0;
    }

    void Run() {
        if (!mem.processHandle) {
            std::cerr << "[-] Critical Error: Invalid process handle." << std::endl;
            system("pause");
            return;
        }

        UE4Handler ue(mem);
        std::cout << "[*] Starting Memory Signature Scan (This may take 10-20s)..." << std::endl;
        
        if (!ue.Init()) {
            std::cerr << "[-] Error: Could not find UWorld. Signature might be outdated or game version changed." << std::endl;
            system("pause");
            return;
        }

        std::cout << "[+] Scanner found UWorld pointer offset: 0x" << std::hex << ue.cachedUWorld << std::dec << std::endl;
        std::cout << "[+] Monitoring started. Stay in training field or match." << std::endl;
        
        while (true) {
            if (!cachedRootComponent) {
                if (!ResolvePlayerChain(ue)) {
                    Sleep(1000); 
                    continue;
                }
                std::cout << "\n[+] Local Player Found! Live tracking active." << std::endl;
            }

            Vector3 pos = mem.Read<Vector3>(cachedRootComponent + offsets::RelativeLocation);
            
            if (pos.x == 0 && pos.y == 0 && pos.z == 0) {
                // If position is zero, the player might have died or left match
                cachedRootComponent = 0;
                continue;
            }

            std::printf("[DIAG] Tracker Active | LocalPos: X:%.2f Y:%.2f Z:%.2f     \r", pos.x, pos.y, pos.z);
            fflush(stdout); 

            Sleep(20); 
        }
    }
};

void CleanupTraces() {
    std::cout << "\n[!] Performing native system trace scrub..." << std::endl;
    try {
        std::string prefetchDir = XOR("C:\\Windows\\Prefetch\\");
        std::string prefetchMask = XOR("FRAGPUNKEXTERNAL");
        if (fs::exists(prefetchDir)) {
            for (const auto& entry : fs::directory_iterator(prefetchDir)) {
                if (entry.path().filename().string().find(prefetchMask) != std::string::npos) fs::remove(entry.path());
            }
        }
    } catch (...) {}
    std::cout << "[+] Native cleanup completed." << std::endl;
    Sleep(2000);
}

int main() {
    std::atexit(CleanupTraces);
    Manager mgr;
    mgr.Run();
    return 0;
}
