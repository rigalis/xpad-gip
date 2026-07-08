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
constexpr int    IFACE_NUM    = 0;
constexpr uint8_t EP_IN       = 0x81;
constexpr int    TIMEOUT_MS   = 5000;

const char* UNBIND_PATH = "/sys/bus/usb/drivers/xpad/unbind";
const char* DEVICE_ID   = "1-1:1.0";

static bool unbind_xpad() {
    FILE* f = fopen("/sys/bus/usb/drivers/xpad/unbind", "w");

    if (!f) {
        if (errno == ENOENT) {
            fmt::println("(no xpad driver bound — device already free)");
            return true;
        }
        fmt::println(stderr, "  couldn't open {}: {}", UNBIND_PATH, strerror(errno));
        return false;
    }
    int ret = fprintf(f, "%s", DEVICE_ID);
    fclose(f);

    if (ret < 0) {
        fmt::println(stderr, "  unbind write failed: {}", strerror(errno));
        return false;
    }
    fmt::println("  unbound xpad from {}", DEVICE_ID);
    return true;
}

int main() {
    fmt::println("=== xpad-gip USB probe ===");
    fmt::println("[1] Unbinding xpad driver...");
    if (!unbind_xpad()) {
        fmt::println(stderr, "FATAL: could not unbind xpad. Try as root.");
        return 1;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    fmt::println("[2] Initializing libusb...");
    libusb_context* ctx = nullptr;
    int ret = libusb_init(&ctx);

    if (ret < 0) {
        fmt::println(stderr, "FATAL: libusb_init: {}", libusb_error_name(ret));
        return 1;
    }

    libusb_set_option(ctx, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_WARNING);
    fmt::println("[3] Opening device {:04x}:{:04x}...", VENDOR_ID, PRODUCT_ID);
    libusb_device_handle* handle = libusb_open_device_with_vid_pid(ctx, VENDOR_ID, PRODUCT_ID);

    if (!handle) {
        fmt::println(stderr, "FATAL: device not found. Is it plugged in?");
        libusb_exit(ctx);
        return 1;
    }
    fmt::println("[4] Detaching kernel driver...");

    if (libusb_kernel_driver_active(handle, IFACE_NUM) == 1) {
        ret = libusb_detach_kernel_driver(handle, IFACE_NUM);
        if (ret < 0) {
            fmt::println(stderr, "  detach_kernel_driver: {}", libusb_error_name(ret));
        } else {
            fmt::println("  kernel driver detached");
        }
    } else {
        fmt::println("  no kernel driver on interface {}", IFACE_NUM);
    }
    fmt::println("[5] Claiming interface {}...", IFACE_NUM);
    ret = libusb_claim_interface(handle, IFACE_NUM);
    if (ret < 0) {
        fmt::println(stderr, "FATAL: claim_interface: {}", libusb_error_name(ret));
        libusb_close(handle);
        libusb_exit(ctx);
        return 1;
    }
    
    fmt::println("  interface claimed successfully");
    fmt::println("[6] Reading interrupt transfer (64 bytes, {} ms timeout)...", TIMEOUT_MS);
    uint8_t buf[64] = {};
    int transferred = 0;
    ret = libusb_interrupt_transfer(handle, EP_IN, buf, sizeof(buf), &transferred, TIMEOUT_MS);

    if (ret == LIBUSB_ERROR_TIMEOUT) {
        fmt::println(stderr, "  TIMEOUT — no data in {}ms. Press a button?", TIMEOUT_MS);
    } else if (ret < 0) {
        fmt::println(stderr, "  interrupt_transfer error: {}", libusb_error_name(ret));
    } else {
        fmt::println("  got {} bytes:", transferred);
        for (int i = 0; i < transferred; ++i) {
            fmt::print("{:02x} ", buf[i]);
            if ((i + 1) % 16 == 0) fmt::println("");
        }
        if (transferred % 16 != 0) fmt::println("");
        GipDecoder decoder;
        auto report = decoder.decode(buf, transferred);
        if (report.has_value()) {
            fmt::println("\n[7] Decoded report:");
            fmt::println("  {}", report->to_json());
        } else {
            fmt::println("\n[7] Could not decode as GIP input report (command=0x{:02x})", buf[0]);
        }
    }
    
    libusb_release_interface(handle, IFACE_NUM);
    libusb_close(handle);
    libusb_exit(ctx);

    fmt::println("\nDone.");

    return 0;
}