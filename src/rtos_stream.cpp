#include "rtos_stream.h"
#include <task.h>
#include <queue.h>

// #define TRACE_RTOS_STREAM(x_) printf("%d ms -> RTOS_Stream: ", millis()); x_
#define TRACE_RTOS_STREAM(x_)

QueueHandle_t _xQueueRx = NULL;

RTOS_Stream::RTOS_Stream(USARTClass *usart, int timeout)
:_usart(usart)
{
    _timeout = pdMS_TO_TICKS(timeout);
}

bool RTOS_Stream::init()
{
    TRACE_RTOS_STREAM( printf("init() ... _xMessageBufferTx=%p, _xQueueRx=%p\r\n", _xMessageBufferTx, _xQueueRx); )
    if (_xMessageBufferTx == NULL)
    {
        TRACE_RTOS_STREAM( printf("... create _xMessageBufferTx...\r\n"); )
        _xMessageBufferTx = xMessageBufferCreate( TX_BUFFER_LENGTH );
        TRACE_RTOS_STREAM( printf("... _xMessageBufferTx=%p\r\n", _xMessageBufferTx); )
    if (_xMessageBufferTx == NULL) return false;
    }

    if (_xQueueRx == NULL)
    {
        TRACE_RTOS_STREAM( printf("... create _xQueueRx...\r\n"); )
        _xQueueRx = xQueueCreate( RX_BUFFER_LENGTH, sizeof(uint8_t) );
        TRACE_RTOS_STREAM( printf("... _xQueueRx=%p\r\n", _xQueueRx); )
        if (_xQueueRx == NULL) return false;
    }

    _usart->setRxIrqCallback(&usartRxIrqCallback);

    return true;
}

size_t RTOS_Stream::write(const char* str)
{
    TRACE_RTOS_STREAM( printf("write(\"%s\")\r\n", str); )
    if (str == NULL) return 0;

    TRACE_RTOS_STREAM( printf("... _xMessageBufferTx=%p\r\n", _xMessageBufferTx); )
    if (_xMessageBufferTx == NULL) return 0;

    size_t xBytesSent;
    if( xPortIsInsideInterrupt() )
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE; /* Initialised to pdFALSE. */

        /* Attempt to send the string to the message buffer. */
        xBytesSent = xMessageBufferSendFromISR( _xMessageBufferTx,
                                                ( void * ) str,
                                                strlen( str ),
                                                &xHigherPriorityTaskWoken );

        TRACE_RTOS_STREAM( printf("... %u bytes sent from interrupt.\r\n", xBytesSent); )

        portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
        return xBytesSent;
    }

    xBytesSent = xMessageBufferSend( _xMessageBufferTx,
                               ( void * ) str,
                               strlen( str ),
                               _timeout );
    TRACE_RTOS_STREAM( printf("... %u bytes sent.\r\n", xBytesSent); )
    return xBytesSent;
}

int RTOS_Stream::available()
{
    if (_xQueueRx == NULL) return false;
    return uxQueueMessagesWaiting( _xQueueRx );
}

int RTOS_Stream::read()
{
    if (_xQueueRx == NULL) return 0;

    uint8_t ch;
    BaseType_t rc;

    if( xPortIsInsideInterrupt() )
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        rc = xQueueReceiveFromISR(
            _xQueueRx,
            ( void * ) &ch,
            &xHigherPriorityTaskWoken
        );
        portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
        return (pdTRUE == rc ? 1 : 0);
    }

    return (pdTRUE == xQueueReceive( _xQueueRx, ( void * ) &ch, _timeout ) ? 1 : 0);
}

size_t RTOS_Stream::readBytesUntil( char terminator, char *buffer, size_t length)
{
    TRACE_RTOS_STREAM( printf("readBytesUntil(terminator=0x%02x, buffer=%p, length=%u)\r\n", reinterpret_cast<uint32_t*>(terminator), static_cast<void*>(buffer), length); )

    TRACE_RTOS_STREAM( printf("... _xQueueRx=%p.\r\n", _xQueueRx); )
    if (_xQueueRx == NULL) return 0;

    size_t idx;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    bool insideITR = xPortIsInsideInterrupt();

    for(idx=0; (idx < length) && (idx < RX_BUFFER_LENGTH);)
    {
        if( insideITR )
        {
            if( xQueueReceiveFromISR(
                    _xQueueRx,
                    ( void * ) (buffer + idx),
                    &xHigherPriorityTaskWoken
            ))
            {
                TRACE_RTOS_STREAM( printf("... %c received from ISR.\r\n", buffer[idx]); )
                ++idx;
            }
            else break;
        }
        else
        {
            if( xQueueReceive(
                    _xQueueRx,
                    ( void * ) (buffer + idx),
                    _timeout
            ))
            {
                TRACE_RTOS_STREAM( printf("... %c received.\r\n", buffer[idx]); )
                ++idx;
            }
            else break;
        }

        if( (idx > 0) && (buffer[idx - 1] == terminator) ) break;
    }

    TRACE_RTOS_STREAM( printf("... %u bytes received in total.\r\n", idx); )

    return idx;
}

void RTOS_Stream::workTx(const TickType_t xTicksToWaitBufferReceive)
{
    if (_xMessageBufferTx == NULL) return;

    uint8_t ucRxData[ RX_BUFFER_LENGTH ];
    size_t xReceivedBytes;

    TRACE_RTOS_STREAM( printf("RTOS_Stream::workTx() waiting for message ...\r\n"); )
    // wait indefinitely (without timing out), provided INCLUDE_vTaskSuspend is set to 1
    xReceivedBytes = xMessageBufferReceive( _xMessageBufferTx,
                                            ( void * ) ucRxData,
                                            sizeof( ucRxData ),
                                            xTicksToWaitBufferReceive  );
    TRACE_RTOS_STREAM( printf("RTOS_Stream::workTx() ... %u bytes received. Writing to usart: \r\n", xReceivedBytes); )
    for(size_t i = 0; i < xReceivedBytes; ++i)
    {
        // TRACE_RTOS_STREAM( printf("%c\r\n", ucRxData[i]); )
        _usart->write(ucRxData[i]);
    }
    TRACE_RTOS_STREAM( printf("RTOS_Stream::workTx() Writing to usart done\r\n"); )
}

void usartRxIrqCallback(uint8_t ch)
{
    // TRACE_RTOS_STREAM( printf("RTOS_Stream::usartRxIrqCallback(ch=%c)\r\n"); )
    if(_xQueueRx != NULL)
    {
        BaseType_t xHigherPriorityTaskWoken;
        xQueueSendFromISR( _xQueueRx, &ch, &xHigherPriorityTaskWoken );
        if( xHigherPriorityTaskWoken )
        {
            portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
        }
    }
}