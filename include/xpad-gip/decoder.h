#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <optional>
// GIP (Game Input Protocol) packet structure(4-byte header + payload)
//
// Header:
//   [0] command    — 0x20 = input report, 0x04 = identify, 0x05 = power
//   [1] flags      — 0x20 = GIP_OPT_INTERNAL
//   [2] sequence lo
//   [3] [7:4] = payload_len (DWORDs), [3:0] = sequence hi

constexpr size_t GIP_HEADER_SIZE = 4;
constexpr size_t GIP_PAYLOAD_OFFSET = GIP_HEADER_SIZE;
// Payload offsets for input reports (serial protocol variant, no report_id byte)
constexpr size_t GIP_BUTTONS  = 0;   // bytes 0-1 (uint16 LE)
constexpr size_t GIP_LT       = 2;   // bytes 2-3 (uint16 LE)
constexpr size_t GIP_RT       = 4;   // bytes 4-5 (uint16 LE)
constexpr size_t GIP_LX       = 6;   // bytes 6-7 (sint16 LE)
constexpr size_t GIP_LY       = 8;   // bytes 8-9 (sint16 LE)
constexpr size_t GIP_RX       = 10;  // bytes 10-11 (sint16 LE)
constexpr size_t GIP_RY       = 12;  // bytes 12-13 (sint16 LE)
// Button bitmask positions in the 16-bit buttons field at GIP_BUTTONS
// These map physical buttons to bits. RS (bit 3) is intentionally
// non-contiguous — this matches real GIP hardware behavior.
constexpr uint16_t GIP_SYNC        = 1 << 0;
constexpr uint16_t GIP_MENU        = 1 << 1;
constexpr uint16_t GIP_VIEW        = 1 << 2;
constexpr uint16_t GIP_A           = 1 << 4;
constexpr uint16_t GIP_B           = 1 << 5;
constexpr uint16_t GIP_X           = 1 << 6;
constexpr uint16_t GIP_Y           = 1 << 7;
constexpr uint16_t GIP_DPAD_UP     = 1 << 8;
constexpr uint16_t GIP_DPAD_DOWN   = 1 << 9;
constexpr uint16_t GIP_DPAD_LEFT   = 1 << 10;
constexpr uint16_t GIP_DPAD_RIGHT  = 1 << 11;
constexpr uint16_t GIP_LB          = 1 << 13;
constexpr uint16_t GIP_RB          = 1 << 14;
constexpr uint16_t GIP_LS          = 1 << 15;
constexpr uint16_t GIP_RS          = 1 << 3;

struct GipButtonState {
    bool sync, menu, view;
    bool a, b, x, y;
    bool dpad_up, dpad_down, dpad_left, dpad_right;
    bool lb, rb, ls, rs;
};
struct GipAnalogState {
    uint16_t lt;
    uint16_t rt;
    int16_t lx, ly;
    int16_t rx, ry;
};
struct GipReport {
    GipButtonState buttons;
    GipAnalogState analogs;
    uint8_t command;        //GIP command byte (0x20)
    uint16_t sequence;      //packet sequence number
    std::string to_json() const;
};

class GipDecoder {
public:
    std::optional<GipReport> decode(const uint8_t* data, size_t len) const;
    static float trigger_norm(uint16_t raw) { return raw / 1023.0f; }
    static float stick_norm(int16_t raw) { return raw / 32767.0f; }
};