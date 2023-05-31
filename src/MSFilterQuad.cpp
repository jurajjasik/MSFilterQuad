#include "MSFilterQuad.h"

// #define TRACE_MSFQ(x_) printf("%d ms -> MSFilterQuad: ", millis()); x_
#define TRACE_MSFQ(x_)

MSFilterQuad::MSFilterQuad(
    JanasCardQSource3* device,
    StateTuneParRecords& calibPntsRF,
    StateTuneParRecords& calibPntsDC
) {
    // _dcFactor = 0.16784; // 1/2 * a0/q0 - theoretical value for infinity resolution

    _device = device;

    _calibPntsRF = &calibPntsRF;
    _splineRF = new CubicSplineInterp();

    _calibPntsDC = &calibPntsDC;
    _splineDC = new CubicSplineInterp();

    initSplineRF();
    initSplineDC();
}


void MSFilterQuad::initRFFactor(float r0, float frequency) {
    // _rfFactor = RF amp / (m/z)
    // _rfFactor = q0 * pi**2 * atomic_mass / elementary_charge * (r0 * frequency)**2
    // q0 = 0.706
    _rfFactor = 7.22176e-8 * (r0 * r0 * frequency * frequency); // SI units
    _dcFactor = 0.16784 * _rfFactor;  // 1/2 * a0/q0 - theoretical value for infinity resolution
    _MAX_MZ = MAX_RF_AMP / _rfFactor;
}


void _initSpline(const StateTuneParRecords* records, CubicSplineInterp* spline) {
    if (records->_numberTuneParRecs > 2)
        spline->init(records->_tuneParMZ, records->_tuneParVal, records->_numberTuneParRecs);
}


void MSFilterQuad::initSplineRF() {
    _initSpline(_calibPntsRF, _splineRF);
    setMZ(_mz);  // set mz using new calibration values
}


void MSFilterQuad::initSplineDC() {
    _initSpline(_calibPntsDC, _splineDC);
    setMZ(_mz);  // set mz using new calibration values
}


float calculateCalib(
    float mz,
    const StateTuneParRecords* records,
    CubicSplineInterp* spline // can not be const because calcHunt change internal state
) {
    if ( (records->_numberTuneParRecs) < 1 ) // none
        return 0;
    if ( (records->_numberTuneParRecs) < 2 ) // constant
        return records->_tuneParVal[0];
    if ( (records->_numberTuneParRecs) < 3 ) { // linear interpolation
        float dX = records->_tuneParMZ[1] - records->_tuneParMZ[0];
        if ( dX > 0.0 || dX < 0.0 )
            return
                (records->_tuneParVal[1] - records->_tuneParVal[0])
                / dX * ( mz - records->_tuneParMZ[0] )
                + records->_tuneParVal[0];
        return records->_tuneParVal[0];
    }

    return spline->calcHunt(mz); // spline interpolation
}


void MSFilterQuad::setDCOffst(float v)
{
    TRACE_MSFQ( printf("setDCOffst(%d)\r\n", (int)(v * 1000)); )

    float diff = getDCDiff();

    setDC1(v + diff);
    setDC2(v - diff);
}


void MSFilterQuad::setRodPolarityPos(bool v)
{
    TRACE_MSFQ( printf("setRodPolarityPos(%d)\r\n", v); )
    if (_polarity != v) {
        setDCDiff(-getDCDiff());
        _polarity = v;
    }
}


void MSFilterQuad::setDCOn(bool v)
{
    TRACE_MSFQ( printf("setDCOn(%d)\r\n", v); )
    _dcOn = v;
    setMZ(_mz);
}


void MSFilterQuad::setDC1(float v)
{
    TRACE_MSFQ( printf("setDC1(%d)\r\n", (int)(v * 1000)); )
    if(v < MIN_DC)
    {
        v = MIN_DC;
    }
    else if(v > MAX_DC)
    {
        v = MAX_DC;
    }
    _connected = _device->writeDC(1, (int32_t)(v * 1000));
    if (_connected) _dc1 = v;
}


void MSFilterQuad::setDC2(float v)
{
    TRACE_MSFQ( printf("setDC2(%d)\r\n", (int)(v * 1000)); )
    if(v < MIN_DC)
    {
        v = MIN_DC;
    }
    else if(v > MAX_DC)
    {
        v = MAX_DC;
    }
    _connected = _device->writeDC(2, (int32_t)(v * 1000));
    if (_connected) _dc2 = v;
}


float MSFilterQuad::calcMaxMz(void) const {
    return _MAX_MZ;
}


float MSFilterQuad::calcRF(float mz) {
    return _rfFactor * (1.0 + calculateCalib(mz, _calibPntsRF, _splineRF)) * mz;
}

float MSFilterQuad::calcDC(float mz) {
    return _dcFactor * (1.0 + calculateCalib(mz, _calibPntsDC, _splineDC)) * mz;
}

void MSFilterQuad::setMZ(float mz) {
    TRACE_MSFQ( printf("setMZ(%d)\r\n", (int)(mz * 1000)); )
    if(mz < 0.0)
    {
        mz = 0.0;
    }
    else if(mz > _MAX_MZ)
    {
        mz = _MAX_MZ;
    }
    float V = calcRF(mz); // RF amplitude
    float U = calcDC(mz); // DC difference
    setUV(U, V);
    if (_connected) _mz = mz;
}


void MSFilterQuad::setDCDiff(float v)
{
    TRACE_MSFQ( printf("setDCDiff(%d)\r\n", (int)(v * 1000)); )
    float ofst = getDCOffst();
    setDC1(ofst + v);
    setDC2(ofst - v);
}


void MSFilterQuad::setRFAmp(float v)
{
    TRACE_MSFQ( printf("setRFAmp(%d)\r\n", (int)(v * 1000)); )
    if(v < 0.0)
    {
        v = 0.0;
    }
    else if(v > MAX_RF_AMP)
    {
        v = MAX_RF_AMP;
    }
    _connected = _device->writeAC((uint32_t)(v * 1000));
    if (_connected) _rfAmp = v;
}


void MSFilterQuad::setVoltages(float rf, float dc1, float dc2)
{
    TRACE_MSFQ( printf("setVoltages(%d, %d, %d)\r\n", (int)(rf * 1000), (int)(dc1 * 1000), (int)(dc2 * 1000)); )
    if(rf < 0.0)
    {
        rf = 0.0;
    }
    else if(rf > MAX_RF_AMP)
    {
        rf = MAX_RF_AMP;
    }

    if(dc1 < MIN_DC)
    {
        dc1 = MIN_DC;
    }
    else if(dc1 > MAX_DC)
    {
        dc1 = MAX_DC;
    }

    if(dc2 < MIN_DC)
    {
        dc2 = MIN_DC;
    }
    else if(dc2 > MAX_DC)
    {
        dc2 = MAX_DC;
    }

    _connected = _device->writeVoltages(
        (int32_t)(dc1 * 1000),
        (int32_t)(dc2 * 1000),
        (uint32_t)(rf * 1000)
    );
    if (_connected) {
        _dc1 = dc1;
        _dc2 = dc2;
        _rfAmp = rf;
    }
}


void MSFilterQuad::setUV(float u, float v) {
    TRACE_MSFQ( printf("setUV(%d, %d)\r\n", (int)(u * 1000), (int)(v * 1000)); )
    float dcOffst = getDCOffst();
    float dc1 = dcOffst;
    float dc2 = dcOffst;

    if (_dcOn) {
        if (_polarity) {
            dc1 += u;
            dc2 -= u;
        }
        else {
            dc1 -= u;
            dc2 += u;
        }
    }
    setVoltages(v, dc1, dc2);
}


MSFilterQuad3::MSFilterQuad3(
    JanasCardQSource3* device,
    StateTuneParRecords* calibPntsRF,
    StateTuneParRecords* calibPntsDC
)
{
    _device = device;

    _msfq[0] = MSFilterQuad(
        device,
        calibPntsRF[0],
        calibPntsDC[0]
    );

    _msfq[1] = MSFilterQuad(
        device,
        calibPntsRF[1],
        calibPntsDC[1]
    );

    _msfq[2] = MSFilterQuad(
        device,
        calibPntsRF[2],
        calibPntsDC[2]
    );
}

bool MSFilterQuad3::init(float r0)
{
    int delay_ms = 10;
    int delay_ms2 = 10;

    // Activate RS485 mode
    if (!_device->writeRSMode(1)) return false;
    delay(delay_ms);
    //turn off RF and DC
    if (!_device->writeVoltages(0, 0, 0)) return false;
    delay(delay_ms);

    // read frequencies of all 3 ranges stored in the device n
    // and calculate RF calibration factor
    for(int i = 0; i < 3; ++i)
    {
        if (!_device->writeFreqRange(i)) return false;
        delay(delay_ms2);
        int32_t f = _device->readFreq();
        delay(delay_ms);
        if (f < 0) return false;
        _msfq[i].initRFFactor(r0, (float)f * 100.0);
        _msfq[i].setVoltages(0, 0, 0);
        if (!_msfq[i].isConnected()) return false;
    }

    _freqRange = 2;

    return true;
}

bool MSFilterQuad3::setFreqRangeIdx(int32_t freqRange){
    bool rc = _device->writeFreqRange(freqRange);
    if(rc) _freqRange = freqRange;
    return rc;
}
