/*****************************************************************//**
 * \file   JanasCardQSource3.h
 * \brief  Arduino library for communication with QSsource3 (JanasCard).
 * 
 * \author jasik
 * \date   December 2022
 *********************************************************************/
 
#ifndef JanasCardQSource3_H_
#define JanasCardQSource3_H_

#include <Arduino.h>

// #define TEST

#define Q_SOURCE3_SERIAL_BAUD_RATE 1500000

#define Q_SOURCE3_MAX_DC 75000
#define Q_SOURCE3_MIN_DC -75000
#define Q_SOURCE3_MAX_AC 650000
#define Q_SOURCE3_MAX_FREQ 28000000
#define Q_SOURCE3_MIN_FREQ 2000000

void initCommJanasCardQSource3(void);

/// <summary>
/// A low-level class for communication with QSsource3 (JanasCard).
/// 
/// RS422/485 uses these parameters: speed 1 MBaud, 8 bits, 1 stop bit, no parity. An implicit setting is
/// full - duplex ie.RS422, it is possible to switch to half - duplex – RS485 via a software command.
/// Connection of Canon 9
/// 1, 2 – A, B – receiver RS422 or Rx / Tx RS485
/// 3, 4 – A, B – transmitter RS422
/// 5 – ground – unnecessary if control equipment is also galvanically connected to ground.
/// </summary>
class JanasCardQSource3 {
    private:
        Stream* _comm = &Serial2; // e.g. Serial1

        bool _query(const char* query, char* buffer, size_t buff_len);
        bool _queryOK(const char* query);
        void _clearBuffer(void);
    
    public:
        /// <summary>
        /// Constructor.
        /// </summary>
        // JanasCardQSource3(void);

        /// <summary>
        /// Communication test.
        /// </summary>
        /// <param name=""></param>
        /// <returns>true if succeeded</returns>
        bool readTest(void);

        /// <summary>
        /// Reads serial number of QSource3, returns three character string.
        /// </summary>
        /// <param name="buffer"> - three character string</param>
        /// <returns>true if succeeded</returns>
        bool readSerialNo(char* buffer, size_t buff_len);

        /// <summary>
        /// Switches between RS485/422.
        /// 
        /// QSource3 listens and switches to transmit mode only for sending
        /// answers. Default state after power-up is RS422.
        /// </summary>
        /// <param name="value"> - for i=0 is communication RS422 ie. full duplex, for i=1 is
        /// communication type RS485</param>
        /// <returns>true if succeeded</returns>
        bool writeRSMode(uint32_t value);

        /// <summary>
        /// Setting of DC voltage.
        /// </summary>
        /// <param name="output"> - 1 or 2</param>
        /// <param name="value"> - DC voltage in mV. The
        /// mentioned resolution is only numerical representation, 
        /// the used DA converters have resolution 16
        /// bits = +-32000 LSB, so the real resolution is 2.3 mV.</param>
        /// <returns>true if succeeded</returns>
        bool writeDC(uint32_t output, int32_t value);

        /// <summary>
        /// Sets AC voltage.
        /// </summary>
        /// <param name="value">- peak to peak value in mV, range is 0 to 650000, 
        /// and corresponding AC voltage is 0 to 650.000 Vpp on the output against ground.
        /// The real resolution is again 16 bits, so LSB is 9.4 mV.</param>
        /// <returns>true if succeeded</returns>
        bool writeAC(uint32_t value);

        /// <summary>
        /// Set both DC voltages and AC voltage together
        /// </summary>
        /// <param name="dc1"> - output 1 DC voltage</param>
        /// <param name="dc2"> - output 2 DC voltage</param>
        /// <param name="ac">- AC amplitude</param>
        /// <returns>true if succeeded</returns>
        bool writeVoltages(int32_t dc1, int32_t dc2, uint32_t ac);

        /// <summary>
        /// Changes resonant frequency and corresponding mass measurement range of the
        /// quadrupole.
        /// </summary>
        /// <param name="range">- has range 0-2, where 0 is the highest range typically 1050 kHz, 
        /// 1 – 480 kHz, 2 – 240 kHz.</param>
        /// <returns>true if succeeded</returns>
        bool writeFreqRange(uint32_t range);

        /// <summary>
        /// Sets the frequency of generator at the actual range given by the <see cref="writeFreqRange()"/>.
        /// </summary>
        /// <param name="value">- frequency in hundreds of Hz. E.g. 4990 corresponds to 499.0 kHz.</param>
        /// <returns>true if succeeded</returns>
        bool writeFreq(uint32_t value);

        /// <summary>
        /// stores actual value of frequency at the actual range given by the <see cref="writeFreqRange()"/>
        /// into Flash memory. This value is read after power on.
        /// </summary>
        /// <param name=""></param>
        /// <returns>true if succeeded</returns>
        bool storeFreq(void);

        /// <summary>
        /// Reads actual frequency at the actual range given by the command <see cref="writeFreqRange()"/>. 
        /// </summary>
        /// <param name=""></param>
        /// <returns>frequency in hundreds of Hz or -1 in case of communication error.</returns>
        int32_t readFreq(void);

        /// <summary>
        /// Reads value of the excitation current.
        /// </summary>
        /// <param name=""></param>
        /// <returns>value of current. A valid value is in the range 0 to 3300. 
        /// The real value of the excitation current in mA is I = val / 10.
        /// If the current is out of range, val is 9999. 
        /// In case of communication error -1 is returned.</returns>
        int32_t readCurrent(void);
};


#endif /* JanasCardQSource3_H_ */

