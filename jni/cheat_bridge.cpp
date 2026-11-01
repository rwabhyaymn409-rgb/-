// ============================================================
// FILE: jni/cheat_bridge.cpp
// Dual-Mode Bridge: Kernel Driver (/dev/ff_cheat) + Root (/proc/<pid>/mem)
// ============================================================
#pragma once
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <initializer_list>
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
    int      fd       = -1;
    int      mem_fd   = -1;
    pid_t    pid      = -1;
    uint64_t base     =  0;
    bool     isKernel = false;

    pid_t FindProcessId(const char* pkg_name) {
        DIR* dir = opendir("/proc");
        if (!dir) return -1;
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_type != DT_DIR) continue;
            char* endptr;
            long p = strtol(entry->d_name, &endptr, 10);
            if (*endptr != '\0') continue;

            char cmdline_path[64];
            snprintf(cmdline_path, sizeof(cmdline_path), "/proc/%ld/cmdline", p);
            FILE* f = fopen(cmdline_path, "r");
            if (f) {
                char cmdline[256] = {};
                if (fgets(cmdline, sizeof(cmdline), f)) {
                    if (strstr(cmdline, pkg_name)) {
                        fclose(f);
                        closedir(dir);
                        return (pid_t)p;
                    }
                }
                fclose(f);
            }
        }
        closedir(dir);
        return -1;
    }

    uint64_t GetModuleBase(pid_t target_pid, const char* mod_name) {
        char maps_path[64];
        snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", target_pid);
        FILE* f = fopen(maps_path, "r");
        if (!f) return 0;
        char line[512];
        uint64_t mod_base = 0;
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, mod_name)) {
                mod_base = strtoull(line, nullptr, 16);
                break;
            }
        }
        fclose(f);
        return mod_base;
    }

    bool Init() {
        // 1. Try Kernel driver first
        fd = open(DEV_PATH, O_RDWR);
        if (fd >= 0) {
            char name[64] = "com.dts.freefireth";
            if (ioctl(fd, IOCTL_GETPID, name) == 0) {
                memcpy(&pid, name, sizeof(pid));
                BaseQuery bq{};
                bq.pid = pid;
                strncpy(bq.mod, "libil2cpp.so", sizeof(bq.mod));
                if (ioctl(fd, IOCTL_GETBASE, &bq) == 0 && bq.base != 0) {
                    base = bq.base;
                    isKernel = true;
                    ioctl(fd, IOCTL_HIDE, 0);
                    __android_log_print(ANDROID_LOG_INFO, TAG, "[+] Kernel Mode: pid=%d base=0x%llX", pid, (unsigned long long)base);
                    return true;
                }
            }
            close(fd);
            fd = -1;
        }

        // 2. Fallback: Direct Root Mode (/proc/<pid>/mem + process_vm_readv)
        isKernel = false;
        pid = FindProcessId("com.dts.freefireth");
        if (pid <= 0) {
            pid = FindProcessId("com.dts.freefiremax");
        }
        if (pid <= 0) return false;

        base = GetModuleBase(pid, "libil2cpp.so");
        if (base == 0) return false;

        if (mem_fd < 0) {
            char mem_path[64];
            snprintf(mem_path, sizeof(mem_path), "/proc/%d/mem", pid);
            mem_fd = open(mem_path, O_RDWR);
        }

        __android_log_print(ANDROID_LOG_INFO, TAG,
            "[+] Root Mode Active: pid=%d base=0x%llX (mem_fd=%d)",
            pid, (unsigned long long)base, mem_fd);

        return true;
    }

    template<typename T>
    T Read(uint64_t addr) {
        T val{};
        if (isKernel && fd >= 0) {
            KMem km{ pid, addr, &val, sizeof(T) };
            ioctl(fd, IOCTL_READ, &km);
            return val;
        }

        if (mem_fd >= 0) {
            if (pread64(mem_fd, &val, sizeof(T), (off64_t)addr) == sizeof(T)) {
                return val;
            }
        }

        struct iovec local_iov { &val, sizeof(T) };
        struct iovec remote_iov { reinterpret_cast<void*>(addr), sizeof(T) };
        process_vm_readv(pid, &local_iov, 1, &remote_iov, 1, 0);
        return val;
    }

    template<typename T>
    void Write(uint64_t addr, T val) {
        if (isKernel && fd >= 0) {
            KMem km{ pid, addr, &val, sizeof(T) };
            ioctl(fd, IOCTL_WRITE, &km);
            return;
        }

        if (mem_fd >= 0) {
            if (pwrite64(mem_fd, &val, sizeof(T), (off64_t)addr) == sizeof(T)) {
                return;
            }
        }

        struct iovec local_iov { &val, sizeof(T) };
        struct iovec remote_iov { reinterpret_cast<void*>(addr), sizeof(T) };
        process_vm_writev(pid, &local_iov, 1, &remote_iov, 1, 0);
    }

    uint64_t Chain(uint64_t base_addr, std::initializer_list<uint64_t> offs) {
        uint64_t cur = base_addr;
        for (auto o : offs) {
            cur = Read<uint64_t>(cur + o);
            if (!cur) return 0;
        }
        return cur;
    }

    ~KernelBridge() {
        if (fd >= 0) close(fd);
        if (mem_fd >= 0) close(mem_fd);
    }
};

KernelBridge g_bridge;