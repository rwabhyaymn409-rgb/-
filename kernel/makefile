# ============================================================
# Kbuild — Pixel 6 Pro (raven/raviole) / Tensor G1
# Android 16 uses kernel 6.1 — NOT 5.10
# Branch: android-gs-raviole-6.1-android16
# ============================================================

obj-m += ff_kdriver.o

# ── Toolchain paths ──────────────────────────────────────────
# After repo sync, clang version for 6.1 is r522817 (not r450784d)
KDIR      ?= $(HOME)/pixel6_kernel/aosp
CLANG_DIR ?= $(HOME)/pixel6_kernel/prebuilts/clang/host/linux-x86_64/clang-r522817

# ── ARM64 / GKI ──────────────────────────────────────────────
ARCH          := arm64
CC            := $(CLANG_DIR)/bin/clang
LD            := $(CLANG_DIR)/bin/ld.lld
CROSS_COMPILE := aarch64-linux-androidkernel-
CLANG_TRIPLE  := aarch64-linux-gnu-

# ── Module flags ─────────────────────────────────────────────
ccflags-y := \
    -O2 \
    -DCONFIG_ANDROID=1 \
    -DCONFIG_KPROBES=1 \
    -DKBUILD_MODNAME='"ff_kdriver"' \
    -Wno-unused-function \
    -Wno-declaration-after-statement \
    -Wno-implicit-function-declaration

# ── Build targets ────────────────────────────────────────────
all:
	$(MAKE) -C $(KDIR) M=$(PWD)        \
		ARCH=$(ARCH)                   \
		CC=$(CC)                       \
		LD=$(LD)                       \
		CLANG_TRIPLE=$(CLANG_TRIPLE)   \
		CROSS_COMPILE=$(CROSS_COMPILE) \
		modules -j$(shell nproc)

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean