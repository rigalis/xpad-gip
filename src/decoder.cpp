#include "xpad-gip/decoder.h"
#include <cstring>
#include <vector>
#include <nlohmann/json.hpp>

static GipButtonState decode_buttons(uint16_t raw) {
    return {
        .sync       = static_cast<bool>(raw & GIP_SYNC),
        .menu       = static_cast<bool>(raw & GIP_MENU),
        .view       = static_cast<bool>(raw & GIP_VIEW),
        .a          = static_cast<bool>(raw & GIP_A),
        .b          = static_cast<bool>(raw & GIP_B),
        .x          = static_cast<bool>(raw & GIP_X),
        .y          = static_cast<bool>(raw & GIP_Y),
        .dpad_up    = static_cast<bool>(raw & GIP_DPAD_UP),
        .dpad_down  = static_cast<bool>(raw & GIP_DPAD_DOWN),
        .dpad_left  = static_cast<bool>(raw & GIP_DPAD_LEFT),
        .dpad_right = static_cast<bool>(raw & GIP_DPAD_RIGHT),
        .lb         = static_cast<bool>(raw & GIP_LB),
        .rb         = static_cast<bool>(raw & GIP_RB),
        .ls         = static_cast<bool>(raw & GIP_LS),
        .rs         = static_cast<bool>(raw & GIP_RS),
    };
}

std::optional<GipReport> GipDecoder::decode(const uint8_t* data, size_t len) const {

    if (!data || len < GIP_HEADER_SIZE)
        return std::nullopt;
    uint8_t command = data[0];
    uint16_t sequence = static_cast<uint16_t>(data[2])
                      | (static_cast<uint16_t>(data[3]) << 8);

    if (command != 0x20)
        return std::nullopt;
    const uint8_t* payload = data + GIP_HEADER_SIZE;
    size_t payload_len = len - GIP_HEADER_SIZE;

    if (payload_len < 14)
        return std::nullopt;
    uint16_t buttons_raw = static_cast<uint16_t>(payload[GIP_BUTTONS])
                         | (static_cast<uint16_t>(payload[GIP_BUTTONS + 1]) << 8);

    uint16_t lt = static_cast<uint16_t>(payload[GIP_LT])
                | (static_cast<uint16_t>(payload[GIP_LT + 1]) << 8);

    uint16_t rt = static_cast<uint16_t>(payload[GIP_RT])
                | (static_cast<uint16_t>(payload[GIP_RT + 1]) << 8);

    int16_t lx = static_cast<int16_t>(payload[GIP_LX])
               | (static_cast<int16_t>(payload[GIP_LX + 1]) << 8);

    int16_t ly = static_cast<int16_t>(payload[GIP_LY])
               | (static_cast<int16_t>(payload[GIP_LY + 1]) << 8);

    int16_t rx = static_cast<int16_t>(payload[GIP_RX])
               | (static_cast<int16_t>(payload[GIP_RX + 1]) << 8);
               
    int16_t ry = static_cast<int16_t>(payload[GIP_RY])
               | (static_cast<int16_t>(payload[GIP_RY + 1]) << 8);

    return GipReport{
        .buttons = decode_buttons(buttons_raw),
        .analogs = { lt, rt, lx, ly, rx, ry },
        .command = command,
        .sequence = sequence,
    };
}