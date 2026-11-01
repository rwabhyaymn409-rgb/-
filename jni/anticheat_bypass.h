// ============================================================
// FILE: app/jni/anticheat_bypass.h
// Forward declarations for AC bypass layer
// ============================================================
#pragma once
#include "cheat_bridge.cpp"   // pulls in KernelBridge + g_bridge

void InitACBypass(KernelBridge& kb);
void SuspendACThread(pid_t game_pid);
void PatchIntegrityTable(KernelBridge& kb, uint64_t il2cpp_base);
void PatchProcMapsRead(KernelBridge& kb, uint64_t il2cpp_base);
void SpoofTracerPid(KernelBridge& kb, uint64_t il2cpp_base);
void BeatTimingChecks(KernelBridge& kb, uint64_t il2cpp_base);
