#include "addons/esp_uart_bridge.h"
#include "storagemanager.h"
#include "helper.h"
#include "config.pb.h"

#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "tusb.h"

// Frame layout (18 bytes):
//  [0]      0xA5 start
//  [1..4]   buttons (uint32_t LE)
//  [5]      dpad
//  [6..7]   lx (uint16_t LE)
//  [8..9]   ly (uint16_t LE)
//  [10..11] rx (uint16_t LE)
//  [12..13] ry (uint16_t LE)
//  [14]     lt
//  [15]     rt
//  [16]     XOR checksum of [1..15]
//  [17]     0x5A end
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
    // tud_init() on Core 0 changes clk_peri, invalidating the baud divisor set in setup().
    // Wait until USB stack is up, then recalculate baud rate for the new clk_peri.
    if (!uartReady) {
        if (!tud_inited()) return;
        uart_set_baudrate(uartInst, baudRate);  // recalc divisor for current clk_peri
        gpio_set_function(txPin, GPIO_FUNC_UART);
        gpio_set_function(rxPin, GPIO_FUNC_UART);
        uartReady = true;
        return;
    }

    Gamepad* gamepad = Storage::getInstance().GetProcessedGamepad();
    const GamepadState& gs = gamepad->state;

    uint8_t frame[FRAME_SIZE];
    frame[0]  = FRAME_START;
    // buttons (32-bit LE)
    frame[1]  = gs.buttons & 0xFF;
    frame[2]  = (gs.buttons >> 8) & 0xFF;
    frame[3]  = (gs.buttons >> 16) & 0xFF;
    frame[4]  = (gs.buttons >> 24) & 0xFF;
    // dpad
    frame[5]  = gs.dpad;
    // lx, ly, rx, ry (16-bit LE each)
    frame[6]  = gs.lx & 0xFF;
    frame[7]  = (gs.lx >> 8) & 0xFF;
    frame[8]  = gs.ly & 0xFF;
    frame[9]  = (gs.ly >> 8) & 0xFF;
    frame[10] = gs.rx & 0xFF;
    frame[11] = (gs.rx >> 8) & 0xFF;
    frame[12] = gs.ry & 0xFF;
    frame[13] = (gs.ry >> 8) & 0xFF;
    // triggers
    frame[14] = gs.lt;
    frame[15] = gs.rt;
    // XOR checksum of payload bytes
    uint8_t checksum = 0;
    for (int i = 1; i <= 15; i++) checksum ^= frame[i];
    frame[16] = checksum;
    frame[17] = FRAME_END;

    uart_write_blocking(uartInst, frame, FRAME_SIZE);
    uart_tx_wait_blocking(uartInst);
}
