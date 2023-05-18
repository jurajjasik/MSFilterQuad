#ifndef MSFilterQuad_h
#define MSFilterQuad_h

#include <Arduino.h>
#include <JanasCardQSource3.h>
#include <CubicSplineInterp.h>

#define MAX_RF_AMP 325.0

#define MAX_NUMBER_OF_TUNE_PAR_RECORDS CSI_MAX_TAB_POINTS

static struct StateTuneParRecords {
    size_t _numberTuneParRecs;
    float _tuneParMZ[MAX_NUMBER_OF_TUNE_PAR_RECORDS];
    float _tuneParVal[MAX_NUMBER_OF_TUNE_PAR_RECORDS];
} _stateTuneParRecords;


/// <summary>
/// High-level class that represents quadrupole mass filter. Uses JanasCardQSource3.
/// </summary>
class MSFilterQuad
{
    friend class MSFilterQuad3;
    
private:
    float _rfFactor;
    float _dcFactor;

    bool _connected = false;

    float _mz = NAN;
    float _dc1 = NAN;
    float _dc2 = NAN;
    float _rfAmp = NAN;
    bool _polarity = true;
    bool _dcOn = true;

    JanasCardQSource3* _device;
    
    StateTuneParRecords* _calibPntsRF;
    StateTuneParRecords* _calibPntsDC;
    
    CubicSplineInterp* _splineRF;
    CubicSplineInterp* _splineDC;

public:
    MSFilterQuad() = default;
    
    /// <summary>
    /// Constructor.
    /// </summary>
    /// <param name="device">- low-level device</param>
    /// <param name="calibPntsRF">- RF amplitude calibration table</param>
    /// <param name="calibPntsDC">- DC difference calibration table</param>
    MSFilterQuad(
        JanasCardQSource3* device,
        StateTuneParRecords& calibPntsRF,
        StateTuneParRecords& calibPntsDC
    );
    
    // Service methods

        /// <summary>
    /// Initialize RF calibration according to the frequency and r0. 
    /// Must be called before the first use of setMZ() or setDCOn().
    /// </summary>
    /// <param name="frequency">- frequency of the quadrupole</param>
    /// <param name="r0">- characteristic radius of the quadrupole</param>
    void initRFFactor(float r0, float frequency);
    
    /// <summary>
    /// Get boolean flag representing connection status.
    /// </summary>
    /// <param name=""></param>
    /// <returns>true if last communication was successfull</returns>
    bool isConnected(void) const { return _connected; }

    /// <summary>
    /// Sets DC voltage of quadrupole rods 1. 
    /// </summary>
    /// <param name="v">- DC voltage in Volts</param>
    void setDC1(float v);

    /// <summary>
    /// Gets DC voltage of quadrupole rods 1.
    /// </summary>
    /// <param name=""></param>
    /// <returns>a casched value of DC voltage</returns>
    float getDC1(void) const { return _dc1; }

    /// <summary>
    /// Sets DC voltage of quadrupole rods 2.
    /// </summary>
    /// <param name="v">- DC voltage in Volts</param>
    void setDC2(float v);

    /// <summary>
    /// Gets DC voltage of quadrupole rods 2.
    /// </summary>
    /// <param name=""></param>
    /// <returns>a casched value of DC voltage</returns>
    float getDC2(void) const { return _dc2; }

    /// <summary>
    /// Sets DC differential voltage of the quadrupole rods referenced to 
    /// the offset of the quadrupole (quadrupole field axis).
    /// Keeps the offset.
    /// 
    /// DC1 = offset + v,
    /// DC2 = offset - v.
    /// </summary>
    /// <param name="v">- DC difference</param>
    void setDCDiff(float v);

    /// <summary>
    /// Gets DC differential voltage of the quadrupole rods referenced to 
    /// the offset of the quadrupole (quadrupole field axis).
    /// </summary>
    /// <param name=""></param>
    /// <returns>(DC1 - DC2) / 2</returns>
    float getDCDiff(void) const { return (_dc1 - _dc2) / 2; }

    /// <summary>
    /// Sets RF amplitude.
    /// </summary>
    /// <param name="v">- RF amplitude, 0 to Vp.</param>
    void setRFAmp(float v);

    /// <summary>
    /// Gets RF amplitude.
    /// </summary>
    /// <param name=""></param>
    /// <returns>RF amplitude, 0 to Vpp.</returns>
    float getRFAmp(void) const { return _rfAmp; }

    /// <summary>
    /// Sets DC and RF voltages.
    /// </summary>
    /// <param name="rf">- RF amplitude, 0 to Vpp.</param>
    /// <param name="dc1">- DC1 voltage in Volts</param>
    /// <param name="dc2">- DC2 voltage in Volts</param>
    void setVoltages(float rf, float dc1, float dc2);

    /// <summary>
    /// Sets DC offset (field axis of the quadrupole). 
    /// Keeps DC difference.
    /// </summary>
    /// <param name="v">- DC offset value in Volts.</param>
    void setDCOffst(float v);

    /// <summary>
    /// Gets DC offset.
    /// </summary>
    /// <param name=""></param>
    /// <returns>DC offset on Volts.</returns>
    float getDCOffst(void) const { return (_dc1 + _dc2) / 2; }

    /// <summary>
    /// Sets rod polarity.
    /// </summary>
    /// <param name="v">- true for positive and false for negative polarity.</param>
    void setRodPolarityPos(bool v);

    /// <summary>
    /// Gets rod polarity.
    /// </summary>
    /// <param name=""></param>
    /// <returns>true for positive and false for negative polarity</returns>
    bool isRodPolarityPos(void) const { return _polarity; }
    
    // mass filter methods
    
    /// <summary>
    /// Sets DC difference and RF amplitude. Keeps DC offset.
    /// The DC offset is set only when internal flag _dcOn is set. See <see cref="setDCOn"></see>.
    /// DC difference is set according to polarity. See <see cref="setRodPolarityPos"></see>.
    /// For positive polarity, u = (DC1 - DC2) / 2.
    /// For negative polarity, u = (DC2 - DC1) / 2.
    /// </summary>
    /// <param name="u">- DC difference</param>
    /// <param name="v">- RF amplitude, 0 to Vp.</param>
    void setUV(float u, float v);

    /// <summary>
    /// Recalculates spline calibration for RF amplitude (m/z calibration).
    /// Must be invoked after each change in <see cref="recordsRF"></see>.
    /// </summary>
    /// <param name=""></param>
    void initSplineRF(void);

    /// <summary>
    /// Recalculates spline calibration for DC difference (quadrupole resolution).
    /// Must be invoked after each change in <see cref="recordsDC"></see>.
    /// </summary>
    /// <param name=""></param>
    void initSplineDC(void);
    
    /// <summary>
    /// Sets m/z.
    /// </summary>
    /// <param name="v"></param>
    void setMZ(float v);

    /// <summary>
    /// Gets cached m/z.
    /// </summary>
    /// <returns></returns>
    float getMZ() const { return _mz; }

    /// <summary>
    /// Turns on/off DC difference.
        /// It activates a mass filter mode. The mz is set according to internal _mz value. 
    /// </summary>
    /// <param name="v">- true - DC on, quadrupole in mass filter mode; false - DC off,  quadrupole in transmition mode.</param>
    void setDCOn(bool v);

    /// <summary>
    /// Checks DC on/off state.
    /// </summary>
    /// <param name=""></param>
    /// <returns>true when DC on, false otherwise.</returns>
    bool isDCOn(void) const { return _dcOn; }
    
    float calcRF(float);
    
    float calcDC(float);
    
    float calcMaxMz(void) const;
    
    const StateTuneParRecords* getCalibPntsRF(void) const { return _calibPntsRF; }
    
    const StateTuneParRecords* getCalibPntsDC(void) const { return _calibPntsDC; }
};


class MSFilterQuad3
{
private:
    MSFilterQuad _msfq[3];
    JanasCardQSource3* _device;
    int32_t _freqRange = -1;

public:
    /// <summary>
    /// Constructor.
    /// </summary>
    /// <param name="device">- low-level device</param>
    /// <param name="calibPntsRF">- a pointer to array of RF amplitude calibration tables (with 3 members)</param>
    /// <param name="calibPntsDC">- a pointer to array of DC difference calibration tables (with 3 members)</param>
    MSFilterQuad3(
        JanasCardQSource3* device,
        StateTuneParRecords* calibPntsRF,
        StateTuneParRecords* calibPntsDC
    );

    /// <summary>
    /// Initialize basic RF calibration of quadrupole MS filters using r0 and RF frequencies 
    /// stored in JanasCardQSource3 hardware device.
    /// </summary>
    /// <param name="r0">- characteristic radius of the quadrupole</param>
    /// <returns>true if succeeded</returns>
    bool init(float r0);
    
    /// <summary>
    /// Changes resonant frequency and corresponding mass measurement range of the
    /// quadrupole MS filter.
    /// </summary>
    /// <param name="freqRange">- has range 0-2, where 0 is the highest range typically 1050 kHz, 
    /// 1 – 480 kHz, 2 – 240 kHz.</param>
    /// <returns>true if succeeded</returns>
    bool setFreqRangeIdx(int32_t freqRange);
    
    int32_t getActualFreqRangeIdx(void) const { return _freqRange; }
    
    /// <summary>
    /// Gets actual quadrupole MS filter.
    /// </summary>
    /// <returns>pointer to <see cref="MSFilterQuad"></see></returns>
    MSFilterQuad* getActualMSFilter (void) { return &(_msfq[_freqRange]); }
    
    MSFilterQuad* getMSFilter (int i) { return &(_msfq[i]); }
};


#endif