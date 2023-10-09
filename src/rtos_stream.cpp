#include "rtos_stream.h"

RTOS_Stream::RTOS_Stream(Stream *stream, int timeout)
:_stream(stream)
{
    _timeout = pdMS_TO_TICKS(timeout);
    _xMessageBufferTx = xMessageBufferCreate( TX_BUFFER_LENGTH );
    _xMessageBufferRx = xMessageBufferCreate( RX_BUFFER_LENGTH );
}

size_t RTOS_Stream::write(const char* str)
{
    if (str == NULL) return 0;

    if( xPortIsInsideInterrupt() )
    {
        size_t xBytesSent;
        BaseType_t xHigherPriorityTaskWoken = pdFALSE; /* Initialised to pdFALSE. */

        /* Attempt to send the string to the message buffer. */
        xBytesSent = xMessageBufferSendFromISR( _xMessageBufferTx,
                                                ( void * ) str,
                                                strlen( str ),
                                                &xHigherPriorityTaskWoken );

        portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
        return xBytesSent;
    }

    return xMessageBufferSend( _xMessageBufferTx,
                               ( void * ) str,
                               strlen( str ),
                               _timeout );
}

int RTOS_Stream::available()
{
    return xMessageBufferIsEmpty( _xMessageBufferRx ) == pdFALSE;
}

int RTOS_Stream::read()
{
    uint8_t ucRxData[ RX_BUFFER_LENGTH ];
    size_t xReceivedBytes;

    if( xPortIsInsideInterrupt() )
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xReceivedBytes = xMessageBufferReceiveFromISR( _xMessageBufferRx,
                                                       ( void * ) ucRxData,
                                                       sizeof( ucRxData ),
                                                       &xHigherPriorityTaskWoken );
        portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
        return xReceivedBytes;
    }

    return xMessageBufferReceive( _xMessageBufferRx,
                                            ( void * ) ucRxData,
                                            sizeof( ucRxData ),
                                            _timeout );
}

size_t RTOS_Stream::readBytesUntil( char terminator, char *buffer, size_t length)
{
    if(xMessageBufferIsEmpty( _xMessageBufferRx ) == pdTRUE)
    {
        return 0;
    }

    uint8_t ucRxData[ RX_BUFFER_LENGTH ];
    size_t xReceivedBytes;
    size_t idx;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if( xPortIsInsideInterrupt() )
    {
        
        xReceivedBytes = xMessageBufferReceiveFromISR( _xMessageBufferRx,
                                                       ( void * ) ucRxData,
                                                       sizeof( ucRxData ),
                                                       &xHigherPriorityTaskWoken );
    }
    else
    {
        xReceivedBytes = xMessageBufferReceive( _xMessageBufferRx,
                                                ( void * ) ucRxData,
                                                sizeof( ucRxData ),
                                                _timeout );
    }
    
    for(idx = 0; (idx < length) && (idx < xReceivedBytes) && (idx < RX_BUFFER_LENGTH); ++idx)
    {
        if( terminator == ucRxData[idx] ) break;
        buffer[idx] = ucRxData[idx];
    }
    
    if( xPortIsInsideInterrupt() ) portYIELD_FROM_ISR( xHigherPriorityTaskWoken );

    return idx;
}

void RTOS_Stream::workTx()
{
    uint8_t ucRxData[ RX_BUFFER_LENGTH ];
    size_t xReceivedBytes;
    
    // wait indefinitely (without timing out), provided INCLUDE_vTaskSuspend is set to 1
    xReceivedBytes = xMessageBufferReceive( _xMessageBufferTx,
                                            ( void * ) ucRxData,
                                            sizeof( ucRxData ),
                                            portMAX_DELAY  );  
    
    _stream->write(( char * )ucRxData);
}

void RTOS_Stream::workRx(char terminator)
{
    bool isTerminator = false;
    while((_stream->available()) && (_rxIdx < RX_BUFFER_LENGTH))
    {
        char c = _stream->read();
        _rxBuffer[_rxIdx++] = c;
        if(c == terminator) 
        {
            isTerminator = true;
            break;
        }
    }
    
    if(isTerminator || (_rxIdx == RX_BUFFER_LENGTH))
    {
        xMessageBufferSend( _xMessageBufferRx,
                           ( void * ) _rxBuffer,
                           _rxIdx,
                           _timeout );
        _rxIdx = 0;
    }
}
