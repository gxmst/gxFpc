#pragma once
#include "Memory.hpp"
#include <string>
#include <vector>

#define __ROR4__(x, y) ((unsigned int)(x) >> (y) | (unsigned int)(x) << (32 - (y)))

namespace offsets {
    // Primary signature (from original project)
    inline const char* UWORLD_SIG_1 = "\x48\x8B\x0D\x00\x00\x00\x00\x31\xD2\x41\xB0\x01\xE8\x00\x00\x00\x00\x48\x85\xC0";
    inline const char* UWORLD_MASK_1 = "xxx????xxxxxx????xxx";
    
    // Fallback signature A (Common UE4 pattern)
    inline const char* UWORLD_SIG_2 = "\x48\x8B\x05\x00\x00\x00\x00\x4D\x8B\xC2";
    inline const char* UWORLD_MASK_2 = "xxx????xxx";

    // Fallback signature B (Another common pattern)
    inline const char* UWORLD_SIG_3 = "\x48\x8B\x15\x00\x00\x00\x00\x48\x85\xD2\x74\x00\x48\x8B\x02\x48\x8D\x0D";
    inline const char* UWORLD_MASK_3 = "xxx????xxxx?xxxxx";

    // FNamePool Signature (Global Name Pool)
    inline const char* FNAME_SIG = "\x48\x8D\x0D\x00\x00\x00\x00\xE8\x00\x00\x00\x00\xC6\x05\x00\x00\x00\x00\x01\x48\x63\xC7";
    inline const char* FNAME_MASK = "xxx????x????xx????xxxx";
    
    // Member offsets
    inline uintptr_t OwningGameInstance = 0x598;
    inline uintptr_t GameState = 0x5C0;
    inline uintptr_t LocalPlayers = 0x110;
    inline uintptr_t PlayerController = 0x40;
    inline uintptr_t AcknowledgedPawn = 0x408;
    inline uintptr_t RootComponent = 0x2A0;
    inline uintptr_t RelativeLocation = 0xC0; // Standard UE4 but check encryption
}

class UE4Handler {
private:
    Memory& mem;

public:
    uintptr_t cachedUWorld = 0;

    UE4Handler(Memory& m) : mem(m) {}

    // Ported decryption logic
    uintptr_t Decrypt(uintptr_t encrypted, uint32_t key, uint8_t ror) {
        if (!encrypted) return 0;
        return ((encrypted & 0xFFFFFFFF00000000u)) | (unsigned int)__ROR4__(encrypted ^ key, ror);
    }

    bool Init() {
        bool worldFound = false;

        // Try signature 1
        uintptr_t addr = mem.PatternScan(offsets::UWORLD_SIG_1, offsets::UWORLD_MASK_1);
        if (addr) {
            cachedUWorld = mem.GetRIPRelative(addr, 3, 7);
            if (cachedUWorld) {
                std::cout << "[+] Found UWorld using Sig 1: 0x" << std::hex << cachedUWorld << std::dec << std::endl;
                worldFound = true;
            }
        }

        if (!worldFound) {
            // Try signature 2
            addr = mem.PatternScan(offsets::UWORLD_SIG_2, offsets::UWORLD_MASK_2);
            if (addr) {
                cachedUWorld = mem.GetRIPRelative(addr, 3, 7);
                if (cachedUWorld) {
                    std::cout << "[+] Found UWorld using Sig 2: 0x" << std::hex << cachedUWorld << std::dec << std::endl;
                    worldFound = true;
                }
            }
        }

        if (!worldFound) {
            // Try signature 3
            addr = mem.PatternScan(offsets::UWORLD_SIG_3, offsets::UWORLD_MASK_3);
            if (addr) {
                cachedUWorld = mem.GetRIPRelative(addr, 3, 7);
                if (cachedUWorld) {
                    std::cout << "[+] Found UWorld using Sig 3: 0x" << std::hex << cachedUWorld << std::dec << std::endl;
                    worldFound = true;
                }
            }
        }

        // Diagnostic: Try to find FNamePool just to verify scanner health
        uintptr_t nameAddr = mem.PatternScan(offsets::FNAME_SIG, offsets::FNAME_MASK);
        if (nameAddr) {
            uintptr_t cachedNamePool = mem.GetRIPRelative(nameAddr, 3, 7); // Usually +3 Rip
            std::cout << "[+] FNamePool detected at: 0x" << std::hex << cachedNamePool << std::dec << " (Scanner is Working!)" << std::endl;
        } else {
            std::cout << "[-] FNamePool not found. (Scanner may be missing target module data)" << std::endl;
        }

        return worldFound;
    }

    uintptr_t GetUWorld() {
        uintptr_t uworldPtr = 0;
        mem.Read(cachedUWorld, uworldPtr);
        return uworldPtr;
    }

    // FName resolution (Placeholder for now, but signature based)
    std::string GetNameFromIndex(int index) {
        // Implementation based on FNamePool later
        return "NotImplemented";
    }
};
