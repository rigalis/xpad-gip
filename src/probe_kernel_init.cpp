#include <libusb-1.0/libusb.h>
#include <fmt/core.h>
#include <thread>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cerrno>

constexpr uint16_t VENDOR_ID  = 0x03f0;
constexpr uint16_t PRODUCT_ID = 0x07a0;
constexpr int      IFACE_NUM  = 0;
constexpr uint8_t  EP_IN      = 0x81;
constexpr uint8_t  EP_OUT     = 0x01;
constexpr int      TIMEOUT_MS = 1000;

static void unbind_xpad() {
    FILE* f = fopen("/sys/bus/usb/drivers/xpad/unbind", "w");
    if (f) { fprintf(f, "%s", "1-1:1.0"); fclose(f); }
}

int main() {
    fmt::println("=== Kernel-mirror init sequence ===");
    unbind_xpad();

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    libusb_context* ctx = nullptr;
    libusb_init(&ctx);
    libusb_set_option(ctx, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_ERROR);
    libusb_device_handle* handle = libusb_open_device_with_vid_pid(ctx, VENDOR_ID, PRODUCT_ID);
   
    if (!handle) { 
        fmt::println(stderr, "not found"); 
        return 1; 
    }
    
    if (libusb_kernel_driver_active(handle, IFACE_NUM) == 1)
        libusb_detach_kernel_driver(handle, IFACE_NUM);

    libusb_claim_interface(handle, IFACE_NUM);

    auto out = [&](const uint8_t* data, int len, const char* label) {
        int t = 0;
        int ret = libusb_interrupt_transfer(handle, EP_OUT,
            const_cast<uint8_t*>(data), len, &t, TIMEOUT_MS);
        fmt::println("  {}: {} {}b", label, libusb_error_name(ret), t);
    };

    // Kernel-mirror sequence from usbmon capture
    // POWER: 05 20 00 01 00  (byte3=0x01, NOT 0x10!)

    fmt::println("\n[1] Sending kernel init packets...");

    const uint8_t pwr[] = {0x05, 0x20, 0x00, 0x01, 0x00};
    out(pwr, 5, "POWER");
    const uint8_t led[] = {0x0a, 0x20, 0x01, 0x03, 0x00, 0x01, 0x14};
    out(led, 7, "LED  ");
    const uint8_t auth[] = {0x06, 0x20, 0x02, 0x02, 0x01, 0x00};
    out(auth, 6, "AUTH ");

    // Drain responses
    fmt::println("\n[2] Drain responses...");
    for (int i = 0; i < 10; ++i) {
        uint8_t buf[64] = {};
        int t = 0;
        int ret = libusb_interrupt_transfer(handle, EP_IN, buf, sizeof(buf), &t, 200);
        if (ret == 0 && t > 0) {
            fmt::print("  [{}] {}b: ", i, t);
            for (int j = 0; j < t && j < 20; ++j) fmt::print("{:02x} ", buf[j]);
            fmt::println("");
            if (buf[0] == 0x20) {
                fmt::println("  >>> INPUT REPORT!");
                break;
            }
        } else {
            break;
        }
    }

    // Read loop for input reports
    fmt::println("\n[3] Reading for input reports (press buttons, 15s)...");
    for (int i = 0; i < 15; ++i) {
        uint8_t buf[64] = {};
        int t = 0;
        int ret = libusb_interrupt_transfer(handle, EP_IN, buf, sizeof(buf), &t, 1000);
        if (ret == 0 && t > 0) {
            fmt::print("[{:3d}] {}b: ", i, t);
            for (int j = 0; j < t && j < 20; ++j) fmt::print("{:02x} ", buf[j]);
            if (t > 20) fmt::print("...");
            if (buf[0] == 0x20) fmt::print(" <<< INPUT");
            fmt::println("");
            if (buf[0] == 0x20) break;
        } else if (ret == LIBUSB_ERROR_TIMEOUT) {
        } else {
            fmt::println("[{:3d}] error: {}", i, libusb_error_name(ret));
            break;
        }
    }

    // If we got here without 0x20, try round 2
    fmt::println("\n[4] Round 2 with incremented seq...");
    
    const uint8_t pwr2[] = {0x05, 0x20, 0x03, 0x01, 0x00};
    out(pwr2, 5, "POWER2");
    const uint8_t led2[] = {0x0a, 0x20, 0x04, 0x03, 0x00, 0x01, 0x14};
    out(led2, 7, "LED2  ");
    const uint8_t auth2[] = {0x06, 0x20, 0x05, 0x02, 0x01, 0x00};
    out(auth2, 6, "AUTH2 ");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    for (int i = 0; i < 15; ++i) {
        uint8_t buf[64] = {};
        int t = 0;
        int ret = libusb_interrupt_transfer(handle, EP_IN, buf, sizeof(buf), &t, 1000);

        if (ret == 0 && t > 0) {
            fmt::print("[{:3d}] {}b: ", i+15, t);
            for (int j = 0; j < t && j < 20; ++j) fmt::print("{:02x} ", buf[j]);
            if (t > 20) fmt::print("...");
            if (buf[0] == 0x20) fmt::print(" <<< INPUT");
            fmt::println("");
            if (buf[0] == 0x20) break;
        } else if (ret == LIBUSB_ERROR_TIMEOUT) {
        }
    }

    libusb_release_interface(handle, IFACE_NUM);
    libusb_close(handle);
    libusb_exit(ctx);
    return 0;
}