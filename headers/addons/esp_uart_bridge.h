#ifndef ESP_UART_BRIDGE_H_
#define ESP_UART_BRIDGE_H_

#include "gpaddon.h"

#ifndef ESP_UART_BRIDGE_ENABLED
#define ESP_UART_BRIDGE_ENABLED 0
#endif

#ifndef ESP_UART_BRIDGE_UART_BLOCK
#define ESP_UART_BRIDGE_UART_BLOCK 1
#endif

#ifndef ESP_UART_BRIDGE_TX_PIN
#define ESP_UART_BRIDGE_TX_PIN -1
#endif

#ifndef ESP_UART_BRIDGE_RX_PIN
#define ESP_UART_BRIDGE_RX_PIN -1
#endif

#ifndef ESP_UART_BRIDGE_BAUD
#define ESP_UART_BRIDGE_BAUD 1000000
#endif

#define EspUartBridgeName "EspUartBridge"

class EspUartBridgeAddon : public GPAddon
{
public:
    virtual bool available();
    virtual void setup();
    virtual void preprocess() {}
    virtual void process();
    virtual void postprocess(bool sent) {}
    virtual void reinit() {}
    virtual std::string name() { return EspUartBridgeName; }
private:
    uart_inst_t* uartInst;
    uint32_t baudRate;
    int txPin;
    int rxPin;
    bool uartReady = false;
};

#endif
