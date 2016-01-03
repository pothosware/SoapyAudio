#include "SoapyAudio.hpp"

#ifdef USE_HAMLIB
RigThread::RigThread() {
    terminated.store(true);
}

RigThread::~RigThread() {

}

#ifdef __APPLE__
void *RigThread::threadMain() {
    terminated.store(false);
    run();
    return this;
};

void *RigThread::pthread_helper(void *context) {
    return ((RigThread *) context)->threadMain();
};
#else
void RigThread::threadMain() {
    terminated.store(false);
    run();
};
#endif

void RigThread::setup(rig_model_t rig_model, std::string rig_file, int serial_rate) {
    rigModel = rig_model;
    rigFile = rig_file;
    serialRate = serial_rate;
};

void RigThread::run() {
    int retcode, status;

    SoapySDR_log(SOAPY_SDR_DEBUG, "Rig thread starting.");    

    rig = rig_init(rigModel);
	strncpy(rig->state.rigport.pathname, rigFile.c_str(), FILPATHLEN - 1);
	rig->state.rigport.parm.serial.rate = serialRate;
	retcode = rig_open(rig);
	char *info_buf = (char *)rig_get_info(rig);
    SoapySDR_logf(SOAPY_SDR_DEBUG, "Rig Info: %s", info_buf);
    
    while (!terminated.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        if (freqChanged.load()) {
            status = rig_get_freq(rig, RIG_VFO_CURR, &freq);
            if (freq != newFreq) {
                freq = newFreq;
                rig_set_freq(rig, RIG_VFO_CURR, freq);
                SoapySDR_logf(SOAPY_SDR_DEBUG, "Set Rig Freq: %f", newFreq);
            }
            
            freqChanged.store(false);
        } else {
            status = rig_get_freq(rig, RIG_VFO_CURR, &freq);
        }
        
        SoapySDR_logf(SOAPY_SDR_DEBUG, "Rig Freq: %f", freq);
    }
    
    rig_close(rig);
    
    SoapySDR_log(SOAPY_SDR_DEBUG, "Rig thread exiting.");    
};

freq_t RigThread::getFrequency() {
    if (freqChanged.load()) {
        return newFreq;
    } else {
        return freq;
    }
}

void RigThread::setFrequency(freq_t new_freq) {
    newFreq = new_freq;
    freqChanged.store(true);
}

void RigThread::terminate() {
    terminated.store(true);
};

bool RigThread::isTerminated() {
    return terminated.load();
}
#endif