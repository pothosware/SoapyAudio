#pragma once

#include <hamlib/rig.h>
#include <hamlib/riglist.h>

#ifdef USE_HAMLIB
struct rigGreater
{
    bool operator()( const struct rig_caps *lx, const struct rig_caps *rx ) const {
        std::string ln(std::string(std::string(lx->mfg_name) + " " + std::string(lx->model_name)));
        std::string rn(std::string(std::string(rx->mfg_name) + " " + std::string(rx->model_name)));
    	return ln.compare(rn)<0;
    }
};

class RigThread {
public:
    RigThread();
    ~RigThread();

    void *pthread_helper(void *context);

#ifdef __APPLE__
    void *threadMain();
#else
    void threadMain();
#endif

    void setup(rig_model_t rig_model, std::string rig_file, int serial_rate);
    void run();

    void terminate();
    bool isTerminated();

    freq_t getFrequency();
    void setFrequency(freq_t new_freq);
    
private:
	RIG *rig;
    rig_model_t rigModel;
    std::string rigFile;
    int serialRate;
    
    freq_t freq;
    freq_t newFreq;
    std::atomic_bool terminated, freqChanged;
};

#endif