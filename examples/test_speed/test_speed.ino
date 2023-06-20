#include <MSFilterQuad.h>
#include <JanasCardQSource3.h>

StateTuneParRecords tuneParRecordsAC;
StateTuneParRecords tuneParRecordsDC;

	
JanasCardQSource3 _qSource3 = JanasCardQSource3();


static MSFilterQuad3 _msfq = MSFilterQuad3(
    &_qSource3,
    &tuneParRecordsAC,
    &tuneParRecordsDC
);


void setup(){
	
	Serial.begin(115200);
	while (!Serial) {
		; // wait for serial port to connect. Needed for native USB port only
	}
	Serial.println("Test MSFilterQuad speed.");
	
	// tuneParRecordsAC._numberTuneParRecs = 1;
	// tuneParRecordsAC._tuneParMZ[0] = 0;
	// tuneParRecordsAC._tuneParVal[0] = 0.01;
	
	// tuneParRecordsDC._numberTuneParRecs = 1;
	// tuneParRecordsDC._tuneParMZ[0] = 0;
	// tuneParRecordsDC._tuneParVal[0] = 0.01;
	
	
	// // 71 us
	// tuneParRecordsAC._numberTuneParRecs = 2;
	// tuneParRecordsAC._tuneParMZ[0] = 0;
	// tuneParRecordsAC._tuneParVal[0] = 0.01;
	// tuneParRecordsAC._tuneParMZ[1] = 400;
	// tuneParRecordsAC._tuneParVal[1] = -0.05;
	
	// tuneParRecordsDC._numberTuneParRecs = 2;
	// tuneParRecordsDC._tuneParMZ[0] = 0;
	// tuneParRecordsDC._tuneParVal[0] = 0.01;
	// tuneParRecordsDC._tuneParMZ[1] = 400;
	// tuneParRecordsDC._tuneParVal[1] = 0.1;
	
	
	// // 103 us
	// tuneParRecordsAC._numberTuneParRecs = 3;
	// tuneParRecordsAC._tuneParMZ[0] = 0;
	// tuneParRecordsAC._tuneParVal[0] = 0.01;
	// tuneParRecordsAC._tuneParMZ[1] = 100;
	// tuneParRecordsAC._tuneParVal[1] = -0.05;
	// tuneParRecordsAC._tuneParMZ[2] = 400;
	// tuneParRecordsAC._tuneParVal[2] = 0.05;
	// _msFilter.initSplineRF();
	
	// tuneParRecordsDC._numberTuneParRecs = 2;
	// tuneParRecordsDC._tuneParMZ[0] = 0;
	// tuneParRecordsDC._tuneParVal[0] = 0.01;
	// tuneParRecordsDC._tuneParMZ[1] = 100;
	// tuneParRecordsDC._tuneParVal[1] = -1.0;
	// tuneParRecordsDC._tuneParMZ[2] = 400;
	// tuneParRecordsDC._tuneParVal[2] = -0.1;
	// _msFilter.initSplineDC();
	
	//
	// randomSeed(0); // 145 us
    
    
	// randomSeed(100); // 145 us
	// for (int i = 0; i < MAX_NUMBER_OF_TUNE_PAR_RECORDS; ++i){
		// float mz = 500.0 * (float)i / MAX_NUMBER_OF_TUNE_PAR_RECORDS;
		// tuneParRecordsDC._tuneParMZ[i] =  mz;
		// tuneParRecordsAC._tuneParMZ[i] =  mz;
		// tuneParRecordsDC._tuneParVal[i] = (float)random(1000) / 1000.0;
		// tuneParRecordsAC._tuneParVal[i] = (float)random(1000) / 1000.0;
	// }
	// tuneParRecordsAC._numberTuneParRecs = MAX_NUMBER_OF_TUNE_PAR_RECORDS;
	// tuneParRecordsDC._numberTuneParRecs = MAX_NUMBER_OF_TUNE_PAR_RECORDS;
	
    initCommJanasCardQSource3(0);
    _msfq.init(6e-3);
    
    for(int i = 0; i < 3; ++i)
    {
        MSFilterQuad* m = _msfq.getMSFilter(i);
        m->initSplineRF();
        m->initSplineDC();
        m->setMZ(0.0);
    }
    
	_msfq.setFreqRangeIdx(1);  // middle
}


void loop() {
	long N = 1000;
	long acc = 0;
	for (int i = 0; i < N; ++i) {
		float mz = i % 100;
		long t0 = micros();
		_msfq.getActualMSFilter()->setMZ(mz);
		long t1 = micros();
		acc += (t1 - t0);
	}
	float mean = (float)acc / N;
	
	Serial.print("mean [us] = ");
	Serial.println(mean);
	delay(1000);
}
