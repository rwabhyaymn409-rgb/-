// ============================================================
// FILE: app/jni/cheat_bridge.cpp
// Native bridge — talks to /dev/ff_cheat via ioctl
// Replaces process_vm_readv — pure kernel channel
// ============================================================

#pragma once
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <android/log.h>

#define TAG      "KANHA_BRIDGE"
#define DEV_PATH "/dev/ff_cheat"

#define IOCTL_READ    _IOWR('k', 1, struct KMem)
#define IOCTL_WRITE   _IOW ('k', 2, struct KMem)
#define IOCTL_GETPID  _IOR ('k', 3, int)
#define IOCTL_GETBASE _IOR ('k', 4, unsigned long)
#define IOCTL_HIDE    _IO  ('k', 5)

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
    int      fd   = -1;
    pid_t    pid  = -1;
    uint64_t base =  0;

    bool Init() {
        fd = open(DEV_PATH, O_RDWR);
        if (fd < 0) {
            __android_log_print(ANDROID_LOG_ERROR, TAG,
                "open %s failed: %s", DEV_PATH, strerror(errno));
            return false;
        }

        // Get FF pid
        char name[64] = "com.dts.freefireth";
        if (ioctl(fd, IOCTL_GETPID, name) < 0) return false;
        memcpy(&pid, name, sizeof(pid));
        if (pid <= 0) return false;

        // Get il2cpp base
        BaseQuery bq{};
        bq.pid = pid;
        strncpy(bq.mod, "libil2cpp.so", sizeof(bq.mod));
        if (ioctl(fd, IOCTL_GETBASE, &bq) < 0) return false;
        base = bq.base;

        // Hide driver from AC procfs scan
        ioctl(fd, IOCTL_HIDE, 0);

        __android_log_print(ANDROID_LOG_INFO, TAG,
            "pid=%d base=0x%lX", pid, base);
        return (base != 0);
    }

    template<typename T>
    T Read(uint64_t addr) {
        T val{};
        KMem km{ pid, addr, &val, sizeof(T) };
        ioctl(fd, IOCTL_READ, &km);
        return val;
    }

    template<typename T>
    void Write(uint64_t addr, T val) {
        KMem km{ pid, addr, &val, sizeof(T) };
        ioctl(fd, IOCTL_WRITE, &km);
    }

    uint64_t Chain(uint64_t base_addr,
                   std::initializer_list<uint64_t> offs) {
        uint64_t cur = base_addr;
        for (auto o : offs) {
            cur = Read<uint64_t>(cur + o);
            if (!cur) return 0;
        }
        return cur;
    }

    ~KernelBridge() { if (fd >= 0) close(fd); }
};

static KernelBridge g_bridge;