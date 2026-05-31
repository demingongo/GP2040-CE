#include "addons/esp_uart_bridge.h"
#include "storagemanager.h"
#include "helper.h"
#include "config.pb.h"

#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "tusb.h"

#define FRAME_START   0xA5
#define FRAME_END     0x5A
#define FRAME_SIZE    18

bool EspUartBridgeAddon::available() {
    // DEBUG: force enable — remove after diagnosis
    return true;
}

void EspUartBridgeAddon::setup() {
    // DEBUG: hardcoded to UART0 / GPIO 0+1 — remove after diagnosis
    txPin    = 0;
    rxPin    = 1;
    baudRate = 115200;
    uartInst = uart0;

    uart_init(uartInst, baudRate);
    gpio_set_function(txPin, GPIO_FUNC_UART);
    gpio_set_function(rxPin, GPIO_FUNC_UART);

    const char* banner = "UART OK\r\n";
    uart_write_blocking(uartInst, (const uint8_t*)banner, 9);
    uart_tx_wait_blocking(uartInst);
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
        const char* banner2 = "READY\r\n";
        uart_write_blocking(uartInst, (const uint8_t*)banner2, 7);
        uart_tx_wait_blocking(uartInst);
        return;
    }

    uint8_t frame[FRAME_SIZE];
    frame[0]  = FRAME_START;
    for (int i = 1; i <= 15; i++) frame[i] = (uint8_t)i;
    uint8_t checksum = 0;
    for (int i = 1; i <= 15; i++) checksum ^= frame[i];
    frame[16] = checksum;
    frame[17] = FRAME_END;

    uart_write_blocking(uartInst, frame, FRAME_SIZE);
    uart_tx_wait_blocking(uartInst);
}
