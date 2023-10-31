#include <ErriezSerialTerminal.h>
#include <MSFilterQuad.h>
#include <JanasCardQSource3.h>

// #define TRACE_ME(x_) printf("%d ms -> consoleRTOS: ", millis()); x_
#define TRACE_ME(x_)

#ifdef USE_RTOS

#include <FreeRTOS.h>
#include <task.h>

// LED pin
#define LED_PIN     LED_BUILTIN

// Characteristic radius of the quadrupole in meters
#define Q_RADIUS 4e-3

///////////////////////////////////////////////////////////////////////////////
// Function prototypes
void unknownCommand(const char *command);

void cmdHelp();
void cmdInterrupt();

void cmdInitMSFQ();
void cmdTest();
void cmdFreqRange();
void cmdInfo();
void cmdScanI();
void cmdCalc();

void printErrorCommunication();
void printConsoleChar();

void initSerialTerminal();
void initCommJanasCardQSource3(uint32_t interrupt_priority);

void refreshDisplay();

void cmdOnSilent();
void scanI();
//
///////////////////////////////////////////////////////////////////////////////

// Low priority numbers denote low priority tasks.
const int PRIORITY_TASK_QSOURCE3_RX = 4;
const int PRIORITY_TASK_QSOURCE3_TX = 3;
const int PRIORITY_TASK_TERMINAL = 1;
const int PRIORITY_TASK_UPDATE_DISPLAY = 2;

const size_t STACK_SIZE_TASK_TERMINAL = 512;
const size_t STACK_SIZE_TASK_UPDATE_DISPLAY = 512;
const size_t STACK_SIZE_TASK_QSOURCE3_RX = 512;
const size_t STACK_SIZE_TASK_QSOURCE3_TX = 512;

static_assert(STACK_SIZE_TASK_TERMINAL >= Q_SOURCE3_MIN_STACK_SIZE, "STACK_SIZE_TASK_TERMINAL too small");

void taskTerminal(void *pvParameters);
void taskUpdateDisplay(void *pvParameters);
void taskQsource3Rx(void *pvParameters);
void taskQsource3Tx(void *pvParameters);


static RTOS_Stream streamQSource3 = RTOS_Stream(&Serial2, 100);  // 100 ms timeout

///////////////////////////////////////////////////////////////////////////////
// Mass Filter variables
StateTuneParRecords tuneParRecordsAC[3];
StateTuneParRecords tuneParRecordsDC[3];

JanasCardQSource3 _qSource3 = JanasCardQSource3(&streamQSource3);

MSFilterQuad3 msfq = MSFilterQuad3(&_qSource3, tuneParRecordsAC, tuneParRecordsDC);

//
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
// Serial terminal variables
// Newline character '\r' or '\n'
char newlineChar = '\r';

// Separator character between commands and arguments
char delimiterChar = ' ';

// Create serial terminal object
SerialTerminal term(newlineChar, delimiterChar);
//
///////////////////////////////////////////////////////////////////////////////

// Try to turn on continuously
bool flagTurnOn = false;
bool flagScanI = false;

void setup()
{
    Serial.begin(115200);
    Serial.println("Test");

    streamQSource3.init();
    initCommJanasCardQSource3(0);

    // Initialize the built-in LED
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    xTaskCreate(
        taskTerminal,  // pvTaskCode
        (const portCHAR *)"terminal",  // pcName
        STACK_SIZE_TASK_TERMINAL,  // usStackDepth
        NULL,  // pvParameters
        PRIORITY_TASK_TERMINAL,  // uxPriority
        NULL  // pxCreatedTask
    );

    xTaskCreate(
        taskUpdateDisplay,  // pvTaskCode
        (const portCHAR *)"update display",  // pcName
        STACK_SIZE_TASK_UPDATE_DISPLAY,  // usStackDepth
        NULL,  // pvParameters
        PRIORITY_TASK_UPDATE_DISPLAY,  // uxPriority
        NULL  // pxCreatedTask
    );

    xTaskCreate(
        taskQsource3Rx,  // pvTaskCode
        (const portCHAR *)"QSource3 Rx",  // pcName
        STACK_SIZE_TASK_QSOURCE3_RX,  // usStackDepth
        NULL,  // pvParameters
        PRIORITY_TASK_QSOURCE3_RX,  // uxPriority
        NULL  // pxCreatedTask
    );

    xTaskCreate(
        taskQsource3Tx,  // pvTaskCode
        (const portCHAR *)"QSource3 Tx",  // pcName
        STACK_SIZE_TASK_QSOURCE3_TX,  // usStackDepth
        NULL,  // pvParameters
        PRIORITY_TASK_QSOURCE3_TX,  // uxPriority
        NULL  // pxCreatedTask
    );

    vTaskStartScheduler();

    Serial.println("Failed to start FreeRTOS scheduler");
    while(1);
}

void loop()
{
}

void printErrorCommunication()
{
    Serial.println("Communication error");
}

void initSerialTerminal()
{
    // Initialize serial port
    Serial.println(F("\nQuadrupole Mass Filter."));
    Serial.println(F("Type 'help' to display usage."));
    Serial.println();
    printConsoleChar();

    // Set default handler for unknown commands
    term.setDefaultHandler(unknownCommand);

    // Add command callback handlers
    term.addCommand("?", cmdHelp);
    term.addCommand("help", cmdHelp);
    term.addCommand("init", cmdInitMSFQ);
    term.addCommand("test", cmdTest);
    term.addCommand("fr", cmdFreqRange);
    term.addCommand("info", cmdInfo);
    term.addCommand("i", cmdScanI);
    term.addCommand("calc", cmdCalc);

    //Set interrupt (CTRL-C) command
    term.setInterruptCommand(cmdInterrupt);

    //Enable Char Echoing
    term.setSerialEcho(true);

    //Set Post Command Handler
    term.setPostCommandHandler(printConsoleChar);
}

void printConsoleChar()
{
    Serial.print(F("> "));
}

void unknownCommand(const char *command)
{
    // Print unknown command
    Serial.print(F("Unknown command: "));
    Serial.println(command);
}

void cmdHelp()
{
    // Print usage
    Serial.println(F("Serial terminal usage:"));
    Serial.println(F("  help or ?   Print this usage."));
    Serial.println(F("  init        Initialize the QSource3."));
    Serial.println(F("  test        Test a communication with QSource3."));
    Serial.println(F("  fr [range]  Get/set the frequancy range."));
    Serial.println(F("  info        Print information about device."));
    Serial.println(F("  i           Scan current continuously. Press CTRL-C to break."));
    Serial.println(F("  calc <mz>   Calculate voltages for given mz."));
}

void cmdInterrupt()
{
    // Interrupt
    flagScanI = false;
    flagTurnOn = false;
}

// Called continuously
void refreshDisplay()
{
    if (flagTurnOn)
    {
        cmdOnSilent();
    }
    if (flagScanI)
    {
        scanI();
    }
}

void cmdInitMSFQ()
{
    if (!msfq.init(Q_RADIUS))
    {
        printErrorCommunication();
        return;
    }
    Serial.println("OK");
}

void cmdOnSilent()
{
    if (!msfq.init(Q_RADIUS)) return;
    Serial.println("OK");

    flagTurnOn = false;
    // Turn LED on
    Serial.println(F("Generator on"));
    digitalWrite(LED_PIN, HIGH);
    printConsoleChar();
}

void cmdFreqRange()
{
    // Get argument
    char* arg = term.getNext();
    if (arg == NULL) {
        // Get
        int v = msfq.getActualFreqRangeIdx();
        if (v < 0) {
            Serial.println("ERROR: Not initialized.");
            return;
        }
        Serial.print("fr ");
        Serial.println(v);
        return;
    }

    int val;
    if (sscanf(arg, "%d", &val) != 1) {
        Serial.println(F("ERROR: Cannot convert argument to decimal value."));
        return;
    }

    if (val < 0 || val > 2) {
        Serial.println(F("ERROR: Wrong argument value."));
        return;
    }

    if (!msfq.setFreqRangeIdx(val)) {
        printErrorCommunication();
        return;
    }
    Serial.println("OK");
}

void cmdTest()
{
    TRACE_ME( printf("cmdTest\r\n"); )
    if (!_qSource3.readTest())
    {
        TRACE_ME( printf("... COMMUNICATION ERROR\r\n"); )
        printErrorCommunication();
        return;
    }
    TRACE_ME( printf("... OK\r\n"); )
    Serial.println("OK");
}

void cmdScanI()
{
    flagScanI = true;
}

void cmdInfo()
{
    TRACE_ME( printf("cmdInfo\r\n"); )
    {
        TRACE_ME( printf("... reading serial number ...\r\n"); )
        char buffer[64];
        if (!_qSource3.readSerialNo(buffer, 64))
        {
            TRACE_ME( printf("... COMMUNICATION ERROR\r\n"); )
            printErrorCommunication();
            return;
        }
        Serial.print("   Serial number: ");
        Serial.println(buffer);
    }
    {
        TRACE_ME( printf("... reading frequency ...\r\n"); )
        int x = _qSource3.readFreq();
        if (x < 0)
        {
            TRACE_ME( printf("... COMMUNICATION ERROR\r\n"); )
            printErrorCommunication();
            return;
        }
        Serial.print("   Frequency [kHz]: ");
        Serial.println((float)x / 10.0);
    }
    {
        Serial.print("   Frequency range: ");
        Serial.println(msfq.getActualFreqRangeIdx());
    }
	{
        Serial.print("   Max m/z [u/e]: ");
        Serial.println(msfq.getActualMSFilter()->calcMaxMz());
    }
    {
        int x = _qSource3.readCurrent();
        if (x < 0)
        {
            TRACE_ME( printf("... COMMUNICATION ERROR\r\n"); )
            printErrorCommunication();
            return;
        }
        Serial.print("   Current [mA]: ");
        Serial.println((float)x / 10.0);
    }

	// MS filter info
	{
		Serial.println();
		// too lazy to use Serial.println() in the following
		char buff[128];

		sprintf(buff, "   DC1 [V]: %.2f", msfq.getActualMSFilter()->getDC1());
		Serial.println(buff);

		sprintf(buff, "   DC2 [V]: %.2f", msfq.getActualMSFilter()->getDC2());
		Serial.println(buff);

		sprintf(buff, "   RF 0-p [V]: %.2f", msfq.getActualMSFilter()->getDC2());
		Serial.println(buff);

		sprintf(buff, "   DC diff [V]: %.2f", msfq.getActualMSFilter()->getDCDiff());
		Serial.println(buff);

		sprintf(buff, "   DC ofst [V]: %.2f", msfq.getActualMSFilter()->getDCOffst());
		Serial.println(buff);

		sprintf(buff, "   rod polarity: %s", msfq.getActualMSFilter()->isRodPolarityPos() ? "POS" : "NEG");
		Serial.println(buff);

		Serial.println();

		sprintf(buff, "   m/z [u/e]: %.2f", msfq.getActualMSFilter()->getDCOffst());
		Serial.println(buff);

		sprintf(buff, "   DC on: %s", msfq.getActualMSFilter()->isDCOn() ? "TRUE" : "FALSE");
		Serial.println(buff);
	}
	{
		Serial.print("RF calib pnts (mz:pnt): ");
		const StateTuneParRecords* r = msfq.getActualMSFilter()->getCalibPntsRF();
		for (int i; i < r->_numberTuneParRecs; ++i)
		{
			Serial.print(r->_tuneParMZ[i]);
			Serial.print(":");
			Serial.print(r->_tuneParVal[i]);
			if (i < r->_numberTuneParRecs - 1) Serial.print(", ");
		}
		Serial.println();
	}
	{
		Serial.print("DC calib pnts (mz:pnt): ");
		const StateTuneParRecords* r = msfq.getActualMSFilter()->getCalibPntsDC();
		for (int i; i < r->_numberTuneParRecs; ++i)
		{
			Serial.print(r->_tuneParMZ[i]);
			Serial.print(" :");
			Serial.print(r->_tuneParVal[i]);
			if (i < r->_numberTuneParRecs - 1) Serial.print(" ,");
		}
		Serial.println();
	}

    Serial.println("OK");
}

void scanI()
{
    int x = _qSource3.readCurrent();
    if (x < 0)
    {
        printErrorCommunication();
        return;
    }
    Serial.print("   Current [mA]: ");
    Serial.println((float)x / 10.0);
}

void cmdCalc()
{
    // Get argument
    char* arg = term.getNext();
    if (arg == NULL) {
        Serial.println("ERROR: mz argument missing.");
        return;
    }

    float mz;
    if (sscanf(arg, "%f", &mz) != 1) {
        Serial.println(F("ERROR: Cannot convert argument to decimal value."));
        return;
    }

    if (!msfq.getActualMSFilter()->isConnected()) {
        printErrorCommunication();
        return;
    }

	Serial.println("debug");

	float rf = msfq.getActualMSFilter()->calcRF(mz);
	float dc = msfq.getActualMSFilter()->calcDC(mz);
	Serial.print("RF amp 0-p [V]: "); Serial.println(rf);
	Serial.print("DC [V]: "); Serial.println(dc);
    Serial.println("OK");
}

void taskTerminal(void *pvParameters)
{
    initSerialTerminal();
    Serial.println("Trying to init QSource3. Press CTRL-C to break...");
    flagTurnOn = true;

    for(;;)
    {
        // Read from serial port and handle command callbacks
        term.readSerial();
        vTaskDelay(100);
    }
}


void taskUpdateDisplay(void *pvParameters)
{
    for(;;)
    {
        refreshDisplay();
        vTaskDelay(1000);
    }
}


void taskQsource3Rx(void *pvParameters)
{
    const TickType_t xTicksToWaitStream = pdMS_TO_TICKS(100);
    const TickType_t xTicksToWaitBufferSend = portMAX_DELAY;
    
    for(;;)
    {
        streamQSource3.workRx(
              '\r'  // terminator
            , xTicksToWaitStream
            , xTicksToWaitBufferSend
        );
    }
}

void taskQsource3Tx(void *pvParameters)
{
    const TickType_t xTicksToWaitBufferReceive = portMAX_DELAY;
    
    for(;;)
    {
        streamQSource3.workTx(xTicksToWaitBufferReceive);
    }
}

#else
#error Oops! RTOS mode must be activated!
#endif
