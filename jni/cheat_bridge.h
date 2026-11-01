// ============================================================
// FILE: jni/cheat_bridge.h
// Header for Dual-Mode Kernel + Root Bridge
// ============================================================
#pragma once
#include <stdint.h>
#include <sys/types.h>
#include <initializer_list>

struct KMem {
    pid_t         pid;
    unsigned long addr;
    void*         buf;
    size_t        size;
};

struct BaseQuery {
    pid_t         pid;
    char          mod[128];
    unsigned long base;
};

class KernelBridge {
public:
    int      fd       = -1;
    int      mem_fd   = -1;
    pid_t    pid      = -1;
    uint64_t base     =  0;
    bool     isKernel = false;

    pid_t FindProcessId(const char* pkg_name);
    uint64_t GetModuleBase(pid_t target_pid, const char* mod_name);
    bool Init();

    template<typename T>
    T Read(uint64_t addr);

    template<typename T>
    void Write(uint64_t addr, T val);

    uint64_t Chain(uint64_t base_addr, std::initializer_list<uint64_t> offs);

    ~KernelBridge();
};

extern KernelBridge g_bridge;
