// ============================================================
// FILE: jni/offsets.h
// Free Fire (IL2CPP) Offsets & Structures
// ============================================================
#pragma once
#include <stdint.h>

struct Vector2 {
    float x, y;
};

struct Vector3 {
    float x, y, z;
};

struct Matrix4x4 {
    float m[16];
};

struct PlayerData {
    uint64_t instance = 0;
    Vector3  pos{};
    Vector3  headPos{};
    float    hp = 0.0f;
    float    maxHp = 200.0f;
    bool     isDead = false;
    bool     isTeam = false;
};

struct MenuState {
    bool aimbot      = false;
    bool silent_aim  = false;
    bool esp         = true;
    bool esp_box     = true;
    bool esp_skel    = false;
    bool esp_hp      = true;
    bool no_recoil   = false;
    bool fast_reload = false;
    bool ghost_mode  = false;
    bool speed_hack  = false;

    float aim_fov    = 120.0f;
    float aim_smooth = 5.0f;
};

extern MenuState g_menu;
