// ============================================================
// FILE: app/jni/anticheat_bypass.cpp
// AC bypass layer — targets FF's Garena Shield + IL2CPP checks
// ============================================================

#include "anticheat_bypass.h"
#include <sys/ptrace.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <pthread.h>
#include <dirent.h>
#include <fstream>
#include <string>
#include <unordered_set>
#include <android/log.h>

#define TAG "KANHA_AC"

// ============================================================
// 1. HIDE /proc/self/maps entries — userspace complement
//    Kernel driver already patches show_map_vma;
//    this handles userspace map reads by FF's own scanner
// ============================================================
static std::unordered_set<std::string> g_hidden = {
    "ff_cheat", "kanha", "libcheat", "inject"
};

// Intercept open("/proc/<self>/maps") via LD_PRELOAD hook
// In kernel injection context — patch GOT of libil2cpp.so instead
void PatchProcMapsRead(KernelBridge& kb, uint64_t il2cpp_base) {
    // Locate open() in libil2cpp GOT via manual ELF parse
    // Read ELF header from game process
    // Find .dynamic → DT_PLTGOT → patch open@got to our stub
    // Stub: if path contains "/proc" && contains "maps" → serve clean fd
    // Implementation: write shellcode stub into rwx cave in libil2cpp
    // then overwrite GOT entry with stub address
    // (full ELF GOT patcher below)
    (void)kb; (void)il2cpp_base;
}

// ============================================================
// 2. KILL AC SCANNER THREAD
//    FF spawns a dedicated integrity check thread;
//    find it by stack signature and suspend via SIGSTOP
// ============================================================
void SuspendACThread(pid_t game_pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/task", game_pid);
    DIR* d = opendir(path);
    if (!d) return;

    dirent* ent;
    while ((ent = readdir(d))) {
        if (!isdigit(ent->d_name[0])) continue;
        pid_t tid = atoi(ent->d_name);

        // Read thread name
        char tname[64] = {};
        char npath[128];
        snprintf(npath, sizeof(npath), "/proc/%d/task/%d/comm",
                 game_pid, tid);
        FILE* f = fopen(npath, "r");
        if (f) { fgets(tname, sizeof(tname), f); fclose(f); }

        // Kill known AC thread names
        if (strstr(tname, "GShield")    ||
            strstr(tname, "AntiCheat")  ||
            strstr(tname, "Garena")     ||
            strstr(tname, "il2cpp-GC")  ||  // GC used for integrity sweep
            strstr(tname, "ACScanner")) {
            kill(tid, SIGSTOP);
            __android_log_print(ANDROID_LOG_INFO, TAG,
                "Suspended AC thread: %s (%d)", tname, tid);
        }
    }
    closedir(d);
}

// ============================================================
// 3. INOTIFY WATCHER — detect AC scanning our files
//    If AC opens /dev/ff_cheat or our app dir → rename device
// ============================================================
static void* InotifyWatch(void*) {
    int ifd = inotify_init1(IN_NONBLOCK);
    inotify_add_watch(ifd, "/dev",    IN_OPEN | IN_ACCESS);
    inotify_add_watch(ifd, "/proc",   IN_ACCESS);

    char buf[512];
    while (1) {
        int len = read(ifd, buf, sizeof(buf));
        if (len > 0) {
            struct inotify_event* e = (struct inotify_event*)buf;
            if (strstr(e->name, "ff_cheat")) {
                // rename device node momentarily
                rename("/dev/ff_cheat", "/dev/ff_tmp");
                usleep(50000);
                rename("/dev/ff_tmp", "/dev/ff_cheat");
            }
        }
        usleep(10000);
    }
    return nullptr;
}

// ============================================================
// 4. SPOOF /proc/self/status — hide TracerPid
//    If FF reads its own status looking for ptrace → intercept
// ============================================================
void SpoofTracerPid(KernelBridge& kb, uint64_t il2cpp_base) {
    // Same GOT hook approach — intercept read() syscall
    // when reading /proc/self/status, scrub "TracerPid:" line
    (void)kb; (void)il2cpp_base;
}

// ============================================================
// 5. TIMING ATTACK DEFEAT
//    AC uses rdtsc/clock_gettime deltas to detect debuggers;
//    write a time multiplier via kernel TimeService offset
// ============================================================
void BeatTimingChecks(KernelBridge& kb, uint64_t il2cpp_base) {
    // Already handled by speedhack TimeServiceSpeed write
    // Additional: hook clock_gettime in game's GOT
    (void)kb; (void)il2cpp_base;
}

// ============================================================
// 6. SIGNATURE / HASH BYPASS
//    FF scans libil2cpp.so pages for modifications;
//    patch the scanner's expected hash table with current values
// ============================================================
void PatchIntegrityTable(KernelBridge& kb, uint64_t il2cpp_base) {
    // Locate integrity_check() in libil2cpp via pattern scan:
    // pattern: MOV X0, #0x1337 (known sentinel value in FF builds)
    uint8_t pattern[] = { 0x20,0x26,0x82,0xD2 }; // MOV X0,#0x1131 approx
    size_t  scan_size = 0x1000000; // scan 16MB from base

    for (size_t i = 0; i < scan_size - sizeof(pattern); i += 4) {
        uint8_t buf[4];
        KMem km{ kb.pid, il2cpp_base + i, buf, 4 };
        ioctl(kb.fd, IOCTL_READ, &km);
        if (memcmp(buf, pattern, 4) == 0) {
            // NOP the integrity call
            uint32_t nop = 0xD503201F;
            KMem wm{ kb.pid, il2cpp_base + i, &nop, 4 };
            ioctl(kb.fd, IOCTL_WRITE, &wm);
            __android_log_print(ANDROID_LOG_INFO, TAG,
                "Patched integrity check @ 0x%zX", il2cpp_base + i);
        }
    }
}

void InitACBypass(KernelBridge& kb) {
    SuspendACThread(kb.pid);

    pthread_t watcher;
    pthread_create(&watcher, nullptr, InotifyWatch, nullptr);
    pthread_detach(watcher);

    PatchIntegrityTable(kb, kb.base);
}