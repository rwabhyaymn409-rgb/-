// ============================================================
// FILE: driver/ff_kdriver.c
// Loadable Kernel Module — ARM64 Android
// Injection: /dev/ff_cheat char device
// AC Bypass: hide from procfs, spoof maps, kill AC scans
// ============================================================

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/pid.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/highmem.h>
#include <linux/kallsyms.h>
#include <linux/kprobes.h>
#include <linux/version.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/list.h>
#include <linux/string.h>

// ============================================================
// GKI COMPAT — kallsyms_lookup_name blocked on Android 16
// Use kprobes to resolve the symbol address at runtime
// ============================================================
typedef unsigned long (*kallsyms_fn_t)(const char*);
static kallsyms_fn_t kanha_ksym = NULL;

static int __init resolve_kallsyms(void) {
    struct kprobe kp = { .symbol_name = "kallsyms_lookup_name" };
    int ret = register_kprobe(&kp);
    if (ret < 0) {
        pr_warn("[KANHA] kprobe register failed: %d\n", ret);
        return ret;
    }
    kanha_ksym = (kallsyms_fn_t)kp.addr;
    unregister_kprobe(&kp);
    pr_info("[KANHA] kallsyms resolved @ %px\n", kanha_ksym);
    return 0;
}

// Drop-in replacement — use everywhere instead of kallsyms_lookup_name()
static unsigned long kanha_lookup(const char* name) {
    if (!kanha_ksym) resolve_kallsyms();
    return kanha_ksym ? kanha_ksym(name) : 0;
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("KANHA");
MODULE_DESCRIPTION("FF Kernel Cheat Driver");

#define DEVICE_NAME  "ff_cheat"
#define IOCTL_READ   _IOWR('k', 1, struct KMem)
#define IOCTL_WRITE  _IOW ('k', 2, struct KMem)
#define IOCTL_GETPID _IOR ('k', 3, int)
#define IOCTL_GETBASE _IOR('k', 4, unsigned long)
#define IOCTL_HIDE   _IO  ('k', 5)

struct KMem {
    pid_t        pid;
    unsigned long addr;
    void __user* buf;
    size_t       size;
};

static int    g_major = 0;
static struct class* g_class = NULL;
static struct device* g_dev  = NULL;

// ============================================================
// STEALTH — remove module from /proc/modules & /sys/module
// ============================================================
static struct list_head* g_prev_module = NULL;

static void HideModule(void) {
    g_prev_module = THIS_MODULE->list.prev;
    list_del(&THIS_MODULE->list);
    // Remove from sysfs
    kobject_del(&THIS_MODULE->mkobj.kobj);
}

// ============================================================
// SAFE KERNEL R/W via mm struct
// ============================================================
static int KernelReadMem(pid_t pid, unsigned long addr,
                         void __user* ubuf, size_t size) {
    struct pid*    pid_s  = find_get_pid(pid);
    if (!pid_s) return -ESRCH;
    struct task_struct* task = get_pid_task(pid_s, PIDTYPE_PID);
    put_pid(pid_s);
    if (!task) return -ESRCH;

    struct mm_struct* mm = get_task_mm(task);
    put_task_struct(task);
    if (!mm) return -ENOMEM;

    void* kbuf = kmalloc(size, GFP_KERNEL);
    if (!kbuf) { mmput(mm); return -ENOMEM; }

    int ret = 0;
    unsigned long remaining = size, cur = addr;
    uint8_t* dst = (uint8_t*)kbuf;

    while (remaining > 0) {
        struct page* page = NULL;
        int pg = get_user_pages_remote(mm, cur, 1,
                                       FOLL_FORCE | FOLL_REMOTE,
                                       &page, NULL);
        if (pg <= 0) { ret = -EFAULT; break; }

        void* mapped = kmap(page);
        size_t off   = cur & ~PAGE_MASK;
        size_t chunk = min(remaining, PAGE_SIZE - off);
        memcpy(dst, (uint8_t*)mapped + off, chunk);
        kunmap(page);
        put_page(page);

        dst       += chunk;
        cur       += chunk;
        remaining -= chunk;
    }

    mmput(mm);
    if (!ret && copy_to_user(ubuf, kbuf, size))
        ret = -EFAULT;
    kfree(kbuf);
    return ret;
}

static int KernelWriteMem(pid_t pid, unsigned long addr,
                          void __user* ubuf, size_t size) {
    struct pid*    pid_s  = find_get_pid(pid);
    if (!pid_s) return -ESRCH;
    struct task_struct* task = get_pid_task(pid_s, PIDTYPE_PID);
    put_pid(pid_s);
    if (!task) return -ESRCH;

    struct mm_struct* mm = get_task_mm(task);
    put_task_struct(task);
    if (!mm) return -ENOMEM;

    void* kbuf = kmalloc(size, GFP_KERNEL);
    if (!kbuf) { mmput(mm); return -ENOMEM; }
    if (copy_from_user(kbuf, ubuf, size)) {
        kfree(kbuf); mmput(mm); return -EFAULT;
    }

    int ret = 0;
    unsigned long remaining = size, cur = addr;
    uint8_t* src = (uint8_t*)kbuf;

    while (remaining > 0) {
        struct page* page = NULL;
        int pg = get_user_pages_remote(mm, cur, 1,
                    FOLL_FORCE | FOLL_WRITE | FOLL_REMOTE,
                    &page, NULL);
        if (pg <= 0) { ret = -EFAULT; break; }

        void* mapped = kmap(page);
        size_t off   = cur & ~PAGE_MASK;
        size_t chunk = min(remaining, PAGE_SIZE - off);
        memcpy((uint8_t*)mapped + off, src, chunk);
        set_page_dirty(page);
        kunmap(page);
        put_page(page);

        src       += chunk;
        cur       += chunk;
        remaining -= chunk;
    }

    mmput(mm);
    kfree(kbuf);
    return ret;
}

// ============================================================
// PID + BASE RESOLVER
// ============================================================
static pid_t FindPID(const char* name) {
    struct task_struct* t;
    rcu_read_lock();
    for_each_process(t) {
        if (strcmp(t->comm, name) == 0) {
            pid_t p = t->pid;
            rcu_read_unlock();
            return p;
        }
    }
    rcu_read_unlock();
    return -1;
}

static unsigned long FindModuleBase(pid_t pid, const char* modname) {
    struct pid*         pid_s = find_get_pid(pid);
    if (!pid_s) return 0;
    struct task_struct* task  = get_pid_task(pid_s, PIDTYPE_PID);
    put_pid(pid_s);
    if (!task) return 0;

    struct mm_struct*   mm    = get_task_mm(task);
    put_task_struct(task);
    if (!mm) return 0;

    unsigned long base = 0;
    struct vm_area_struct* vma;
    VMA_ITERATOR(vmi, mm, 0);

    mmap_read_lock(mm);
    for_each_vma(vmi, vma) {
        if (vma->vm_file) {
            char buf[256] = {};
            char* p = d_path(&vma->vm_file->f_path, buf, sizeof(buf));
            if (!IS_ERR(p) && strstr(p, modname)) {
                if (vma->vm_flags & VM_EXEC) {
                    base = vma->vm_start;
                    break;
                }
            }
        }
    }
    mmap_read_unlock(mm);
    mmput(mm);
    return base;
}

// ============================================================
// ANTICHEAT BYPASS — patch /proc/<ac_pid>/maps & hide entries
// ============================================================
// Hook show_map_vma to filter our mapped regions from AC reads
// Requires kallsyms_lookup_name — available on eng/userdebug builds
// On locked bootloaders: patch via kernel .text write (see below)

typedef int (*orig_show_map_vma_t)(struct seq_file*, struct vm_area_struct*);
static orig_show_map_vma_t orig_show_map_vma = NULL;

static int hooked_show_map_vma(struct seq_file* m,
                                struct vm_area_struct* vma) {
    if (vma->vm_file) {
        char buf[256] = {};
        char* p = d_path(&vma->vm_file->f_path, buf, sizeof(buf));
        // Hide injected .so from /proc/maps
        if (!IS_ERR(p) && strstr(p, "ff_cheat")) return 0;
        if (!IS_ERR(p) && strstr(p, "kanha"))    return 0;
    }
    return orig_show_map_vma(m, vma);
}

// Inline patch — write JMP to hooked fn (arm64: 5-insn trampoline)
static void PatchKernelFn(void* target, void* hook) {
    // arm64 absolute jump:
    //   ldr x9, #8
    //   br  x9
    //   .quad <addr>
    uint32_t tramp[4];
    tramp[0] = 0x58000049; // ldr x9, #8
    tramp[1] = 0xD61F0120; // br  x9
    memcpy(&tramp[2], &hook, sizeof(void*));

    // Disable write-protect on kernel pages
    unsigned long flags;
    preempt_disable();
    local_irq_save(flags);

    // arm64: write via fixmap / set_memory_rw equivalent
    // use set_memory_rw from kernel crypto layer
    typedef int (*set_memory_rw_t)(unsigned long, int);
    set_memory_rw_t set_rw =
        (set_memory_rw_t)kanha_lookup("set_memory_rw");
    if (set_rw) set_rw((unsigned long)target & PAGE_MASK, 1);

    memcpy(target, tramp, sizeof(tramp));

    // flush icache
    __flush_icache_range((unsigned long)target,
                         (unsigned long)target + sizeof(tramp));

    local_irq_restore(flags);
    preempt_enable();
}

// ============================================================
// CHAR DEVICE OPS
// ============================================================
static long ff_ioctl(struct file* f, unsigned int cmd, unsigned long arg) {
    switch (cmd) {
    case IOCTL_READ: {
        struct KMem km;
        if (copy_from_user(&km, (void __user*)arg, sizeof(km)))
            return -EFAULT;
        return KernelReadMem(km.pid, km.addr, km.buf, km.size);
    }
    case IOCTL_WRITE: {
        struct KMem km;
        if (copy_from_user(&km, (void __user*)arg, sizeof(km)))
            return -EFAULT;
        return KernelWriteMem(km.pid, km.addr, km.buf, km.size);
    }
    case IOCTL_GETPID: {
        char name[64] = {};
        if (copy_from_user(name, (void __user*)arg, sizeof(name)))
            return -EFAULT;
        pid_t p = FindPID(name);
        if (copy_to_user((void __user*)arg, &p, sizeof(p)))
            return -EFAULT;
        return 0;
    }
    case IOCTL_GETBASE: {
        struct { pid_t pid; char mod[128]; unsigned long base; } q;
        if (copy_from_user(&q, (void __user*)arg, sizeof(q)))
            return -EFAULT;
        q.base = FindModuleBase(q.pid, q.mod);
        if (copy_to_user((void __user*)arg, &q, sizeof(q)))
            return -EFAULT;
        return 0;
    }
    case IOCTL_HIDE:
        HideModule();
        return 0;
    }
    return -EINVAL;
}

static const struct file_operations ff_fops = {
    .owner          = THIS_MODULE,
    .unlocked_ioctl = ff_ioctl,
    .compat_ioctl   = ff_ioctl,
};

// ============================================================
// MODULE INIT / EXIT
// ============================================================
static int __init ff_init(void) {
    g_major = register_chrdev(0, DEVICE_NAME, &ff_fops);
    if (g_major < 0) return g_major;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    g_class = class_create(DEVICE_NAME);
#else
    g_class = class_create(THIS_MODULE, DEVICE_NAME);
#endif
    if (IS_ERR(g_class)) {
        unregister_chrdev(g_major, DEVICE_NAME);
        return PTR_ERR(g_class);
    }

    g_dev = device_create(g_class, NULL,
                          MKDEV(g_major, 0), NULL, DEVICE_NAME);
    if (IS_ERR(g_dev)) {
        class_destroy(g_class);
        unregister_chrdev(g_major, DEVICE_NAME);
        return PTR_ERR(g_dev);
    }

    // Patch show_map_vma to hide injected regions from AC
    orig_show_map_vma = (orig_show_map_vma_t)
        kanha_lookup("show_map_vma");
    if (orig_show_map_vma)
        PatchKernelFn((void*)orig_show_map_vma,
                      (void*)hooked_show_map_vma);

    pr_info("[KANHA] driver loaded, major=%d\n", g_major);
    return 0;
}

static void __exit ff_exit(void) {
    device_destroy(g_class, MKDEV(g_major, 0));
    class_destroy(g_class);
    unregister_chrdev(g_major, DEVICE_NAME);
    pr_info("[KANHA] driver unloaded\n");
}

module_init(ff_init);
module_exit(ff_exit);