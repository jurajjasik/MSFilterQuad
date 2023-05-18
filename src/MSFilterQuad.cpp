#include "MSFilterQuad.h"


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
}


void _initSpline(const StateTuneParRecords* records, CubicSplineInterp* spline) {
    if (records->_numberTuneParRecs > 2)
        spline->init(records->_tuneParMZ, records->_tuneParVal, records->_numberTuneParRecs);
}


void MSFilterQuad::initSplineRF() { 
    _initSpline(_calibPntsRF, _splineRF); 
}


void MSFilterQuad::initSplineDC() {
    _initSpline(_calibPntsDC, _splineDC);
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
    float diff = getDCDiff();
    
    setDC1(v + diff);
    setDC2(v - diff);
}


void MSFilterQuad::setRodPolarityPos(bool v)
{
    if (_polarity != v) {
        setDCDiff(-getDCDiff());
        _polarity = v;
    }
}


void MSFilterQuad::setDCOn(bool v)
{
    _dcOn = v;
    setMZ(_mz);
}


void MSFilterQuad::setDC1(float v)
{
    _connected = _device->writeDC(1, (int32_t)(v * 1000));
    if (_connected) _dc1 = v;
}


void MSFilterQuad::setDC2(float v)
{
    _connected = _device->writeDC(2, (int32_t)(v * 1000));
    if (_connected) _dc2 = v;
}


float MSFilterQuad::calcMaxMz(void) const {
    return MAX_RF_AMP / _rfFactor;
}


float MSFilterQuad::calcRF(float mz) {
    return _rfFactor * (1.0 + calculateCalib(mz, _calibPntsRF, _splineRF)) * mz;
}

float MSFilterQuad::calcDC(float mz) {
    return _dcFactor * (1.0 + calculateCalib(mz, _calibPntsDC, _splineDC)) * mz;
}

void MSFilterQuad::setMZ(float mz) {
    float V = calcRF(mz); // RF amplitude
    float U = calcDC(mz); // DC difference
    setUV(U, V);
    if (_connected) _mz = mz;
}


void MSFilterQuad::setDCDiff(float v)
{
    float ofst = getDCOffst();
    setDC1(ofst + v);
    setDC2(ofst - v);
}


void MSFilterQuad::setRFAmp(float v)
{
    _connected = _device->writeAC((uint32_t)(v * 1000));
    if (_connected) _rfAmp = v;
}


void MSFilterQuad::setVoltages(float rf, float dc1, float dc2)
{
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
    setVoltages(u, dc1, dc2);
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


// MSFilterQuad* MSFilterQuad3::getActualMSFilter(){
    // return &(_msfq[_freqRange]);
// }