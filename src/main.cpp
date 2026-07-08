#include "xpad-gip/decoder.h"

#include <libusb-1.0/libusb.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <fmt/core.h>
#include <thread>
#include <chrono>

constexpr uint16_t VENDOR_ID  = 0x03f0;
constexpr uint16_t PRODUCT_ID = 0x07a0;
constexpr int      IFACE_NUM  = 0;
constexpr uint8_t  EP_IN      = 0x81;
constexpr int      TIMEOUT_MS = 1000;       //shorter: 1s per read

static bool unbind_xpad() {
    FILE* f = fopen("/sys/bus/usb/drivers/xpad/unbind", "w");
    if (!f) {
        if (errno == ENOENT) {
            fmt::println("(no xpad driver bound)");
            return true;
        }
        fmt::println(stderr, "  couldn't open unbind: {}", strerror(errno));
        return false;
    }
    fprintf(f, "%s", "1-1:1.0");
    fclose(f);
    fmt::println("  unbound xpad from 1-1:1.0");
    return true;
}

static void print_hex(const uint8_t* buf, int len) {
    for (int i = 0; i < len; ++i)
        fmt::print("{:02x} ", buf[i]);
}

int main() {
    fmt::println("=== xpad-gip continuous probe ===");
    fmt::println("Reading 200 packets from controller...\n");

    unbind_xpad();

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    libusb_context* ctx = nullptr;
    libusb_init(&ctx);
    libusb_set_option(ctx, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_ERROR);
    libusb_device_handle* handle = libusb_open_device_with_vid_pid(ctx, VENDOR_ID, PRODUCT_ID);

    if (!handle) {
        fmt::println(stderr, "FATAL: device not found");
        return 1;
    }

    if (libusb_kernel_driver_active(handle, IFACE_NUM) == 1)
        libusb_detach_kernel_driver(handle, IFACE_NUM);
    libusb_claim_interface(handle, IFACE_NUM);

    GipDecoder decoder;
    int count = 0;
    
    for (int i = 0; i < 200; ++i) {
        uint8_t buf[64] = {};
        int transferred = 0;
        int ret = libusb_interrupt_transfer(handle, EP_IN, buf, sizeof(buf),
                                            &transferred, TIMEOUT_MS);
        if (ret == 0 && transferred > 0) {
            fmt::print("[{:4d}] {:3d} bytes: ", ++count, transferred);
            print_hex(buf, transferred);
            auto report = decoder.decode(buf, transferred);
            if (report.has_value()) {
                fmt::print("  -> {}", report->to_json());
            } else {
                fmt::print("  (cmd={})", buf[0]);
            }
            fmt::println("");
            // bytes 4-5 often carry 16-bit button mask
            if (transferred >= 6) {
                uint16_t b = static_cast<uint16_t>(buf[4])
                           | (static_cast<uint16_t>(buf[5]) << 8);
                if (b)
                    fmt::println("         raw buttons: 0x{:04x} = {:016b}", b, b);
            }
        } else if (ret == LIBUSB_ERROR_TIMEOUT) {
            //for timeouts occuring when controller is idle
        }
    }

    libusb_release_interface(handle, IFACE_NUM);
    libusb_close(handle);
    libusb_exit(ctx);

    return 0;
}