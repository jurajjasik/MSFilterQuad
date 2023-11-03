#pragma once

#include <Arduino.h>
#include <FreeRTOS.h>
#include <message_buffer.h>

#define RX_BUFFER_LENGTH 128
#define TX_BUFFER_LENGTH 128

void usartRxIrqCallback(uint8_t ch);

class RTOS_Stream
{
private:
    USARTClass* _usart;
    TickType_t _timeout;

    MessageBufferHandle_t _xMessageBufferTx = NULL;
    
    char _rxBuffer[RX_BUFFER_LENGTH];
    size_t _rxIdx = 0;

public:
    RTOS_Stream(USARTClass* stream, int timeout);
    bool init();
    size_t write(const char* str);
    int available();
    int read();
    size_t readBytesUntil( char terminator, char *buffer, size_t length);
    void workTx(const TickType_t xTicksToWaitBufferReceive);
};
