#include "addons/esp_uart_bridge.h"
#include "storagemanager.h"
#include "helper.h"
#include "config.pb.h"

#include "hardware/uart.h"
#include "hardware/gpio.h"

#define FRAME_START   0xA5
#define FRAME_END     0x5A
#define FRAME_SIZE    18

bool EspUartBridgeAddon::available() {
    const EspUartBridgeOptions& options = Storage::getInstance().getAddonOptions().espUartBridgeOptions;
    return options.enabled && isValidPin(options.txPin) && isValidPin(options.rxPin);
}

void EspUartBridgeAddon::setup() {
    const EspUartBridgeOptions& options = Storage::getInstance().getAddonOptions().espUartBridgeOptions;

    txPin    = options.txPin;
    rxPin    = options.rxPin;
    baudRate = options.baudRate;
    uartInst = (options.uartBlock == 0) ? uart0 : uart1;

    uart_init(uartInst, baudRate);
    gpio_set_function(txPin, GPIO_FUNC_UART);
    gpio_set_function(rxPin, GPIO_FUNC_UART);
}

void EspUartBridgeAddon::process() {
    const GamepadState& state = Storage::getInstance().GetProcessedGamepad()->state;

    uint8_t frame[FRAME_SIZE];
    frame[0]  = FRAME_START;

    // buttons: uint32 little-endian (bytes 1–4)
    frame[1]  = (uint8_t)(state.buttons & 0xFF);
    frame[2]  = (uint8_t)((state.buttons >> 8) & 0xFF);
    frame[3]  = (uint8_t)((state.buttons >> 16) & 0xFF);
    frame[4]  = (uint8_t)((state.buttons >> 24) & 0xFF);

    // dpad (byte 5)
    frame[5]  = state.dpad;

    // lx, ly, rx, ry: uint16 little-endian (bytes 6–13)
    frame[6]  = (uint8_t)(state.lx & 0xFF);
    frame[7]  = (uint8_t)((state.lx >> 8) & 0xFF);
    frame[8]  = (uint8_t)(state.ly & 0xFF);
    frame[9]  = (uint8_t)((state.ly >> 8) & 0xFF);
    frame[10] = (uint8_t)(state.rx & 0xFF);
    frame[11] = (uint8_t)((state.rx >> 8) & 0xFF);
    frame[12] = (uint8_t)(state.ry & 0xFF);
    frame[13] = (uint8_t)((state.ry >> 8) & 0xFF);

    // lt, rt (bytes 14–15)
    frame[14] = state.lt;
    frame[15] = state.rt;

    // XOR checksum of bytes 1–15 (byte 16)
    uint8_t checksum = 0;
    for (int i = 1; i <= 15; i++) {
        checksum ^= frame[i];
    }
    frame[16] = checksum;

    frame[17] = FRAME_END;

    uart_write_blocking(uartInst, frame, FRAME_SIZE);
}
