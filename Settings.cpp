/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015 Charles J. Cliffe

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "SoapyAudio.hpp"

#ifdef USE_HAMLIB
std::vector<const struct rig_caps *> SoapyAudio::rigCaps;
#endif

SoapyAudio::SoapyAudio(const SoapySDR::Kwargs &args)
{
    deviceId = -1;

    asFormat = AUDIO_FORMAT_FLOAT32;

    sampleRate = 48000;
    centerFrequency = 0;

    numBuffers = DEFAULT_NUM_BUFFERS;

    agcMode = false;

    bufferedElems = 0;
    resetBuffer = false;
    
    streamActive = false;
    sampleRateChanged.store(false);

    if (args.count("device_id") != 0)
    {
        try {
            deviceId = std::stoi(args.at("device_id"));
        } catch (const std::invalid_argument &) {
        }
        
        int numDevices = dac.getDeviceCount();
        
        if (deviceId < 0 || deviceId >= numDevices)
        {
            throw std::runtime_error(
                    "device_id out of range [0 .. " + std::to_string(numDevices) + "].");
        }
  
        SoapySDR_logf(SOAPY_SDR_DEBUG, "Found Audio device using 'device_id' = %d", deviceId);
    }
    
    if (deviceId == -1) {
        throw std::runtime_error("device_id missing.");
    }

    RtAudio endac;
    
    devInfo = endac.getDeviceInfo(deviceId);
    
#ifdef USE_HAMLIB
    t_Rig = nullptr;
    rigThread = nullptr;
    rigModel = 0;
    rigFile = "";
    rigSerialRate = 0;
    
    if (args.count("rig") != 0 && args.at("rig") != "") {
        try {
            rigModel = std::stoi(args.at("rig"));
        } catch (const std::invalid_argument &) {
            throw std::runtime_error("rig is invalid.");
        }
        if (!args.count("rig_rate")) {
            throw std::runtime_error("rig_rate missing.");
        }
        try {
            rigSerialRate = std::stoi(args.at("rig_rate"));
        } catch (const std::invalid_argument &) {
            throw std::runtime_error("rig_rate is invalid.");
        }

        if (!args.count("rig_port")) {
            throw std::runtime_error("rig_port missing.");
        }
        rigFile = args.at("rig_port");
        checkRigThread();
    }
#endif
}

SoapyAudio::~SoapyAudio(void)
{
#ifdef USE_HAMLIB
    if (rigThread) {
        if (!rigThread->isTerminated()) {
            rigThread->terminate();
        }
        if (t_Rig && t_Rig->joinable()) {
            t_Rig->join();
        }
    }
#endif
}

/*******************************************************************
 * Identification API
 ******************************************************************/

std::string SoapyAudio::getDriverKey(void) const
{
    return "Audio";
}

std::string SoapyAudio::getHardwareKey(void) const
{
    return "Audio";
}

SoapySDR::Kwargs SoapyAudio::getHardwareInfo(void) const
{
    //key/value pairs for any useful information
    //this also gets printed in --probe
    SoapySDR::Kwargs args;

    args["origin"] = "https://github.com/pothosware/SoapyAudio";
    args["device_id"] = std::to_string(deviceId);

    return args;
}

/*******************************************************************
 * Channels API
 ******************************************************************/

size_t SoapyAudio::getNumChannels(const int dir) const
{
    return (dir == SOAPY_SDR_RX) ? 1 : 0;
}

/*******************************************************************
 * Antenna API
 ******************************************************************/

std::vector<std::string> SoapyAudio::listAntennas(const int direction, const size_t channel) const
{
    std::vector<std::string> antennas;
    antennas.push_back("RX");
    // antennas.push_back("TX");
    return antennas;
}

void SoapyAudio::setAntenna(const int direction, const size_t channel, const std::string &name)
{
    // TODO
}

std::string SoapyAudio::getAntenna(const int direction, const size_t channel) const
{
    return "RX";
    // return "TX";
}

/*******************************************************************
 * Frontend corrections API
 ******************************************************************/

bool SoapyAudio::hasDCOffsetMode(const int direction, const size_t channel) const
{
    return false;
}

/*******************************************************************
 * Gain API
 ******************************************************************/

std::vector<std::string> SoapyAudio::listGains(const int direction, const size_t channel) const
{
    //list available gain elements,
    //the functions below have a "name" parameter
    std::vector<std::string> results;

    results.push_back("AUDIO");

    return results;
}

bool SoapyAudio::hasGainMode(const int direction, const size_t channel) const
{
    return true;
}

void SoapyAudio::setGainMode(const int direction, const size_t channel, const bool automatic)
{
    agcMode = automatic;
    SoapySDR_logf(SOAPY_SDR_DEBUG, "Setting Audio AGC: %s", automatic ? "Automatic" : "Manual");
}

bool SoapyAudio::getGainMode(const int direction, const size_t channel) const
{
    return agcMode;
}

void SoapyAudio::setGain(const int direction, const size_t channel, const double value)
{
    //set the overall gain by distributing it across available gain elements
    //OR delete this function to use SoapySDR's default gain distribution algorithm...
    SoapySDR::Device::setGain(direction, channel, value);
}

void SoapyAudio::setGain(const int direction, const size_t channel, const std::string &name, const double value)
{
    if (name == "AUDIO")
    {
        audioGain = value;
        SoapySDR_logf(SOAPY_SDR_DEBUG, "Setting Audio Gain: %f", audioGain);
    }
}

double SoapyAudio::getGain(const int direction, const size_t channel, const std::string &name) const
{
    if ((name.length() >= 2) && (name.substr(0, 2) == "AUDIO"))
    {
        return audioGain;
    }

    return 0;
}

SoapySDR::Range SoapyAudio::getGainRange(const int direction, const size_t channel, const std::string &name) const
{
    return SoapySDR::Range(0, 100);
}

/*******************************************************************
 * Frequency API
 ******************************************************************/

void SoapyAudio::setFrequency(
        const int direction,
        const size_t channel,
        const std::string &name,
        const double frequency,
        const SoapySDR::Kwargs &args)
{
    if (name == "RF")
    {
        centerFrequency = (uint32_t) frequency;
        resetBuffer = true;
        SoapySDR_logf(SOAPY_SDR_DEBUG, "Setting center freq: %d", centerFrequency);
#ifdef USE_HAMLIB
        if (rigThread && !rigThread->isTerminated()) {
            if (rigThread->getFrequency() != frequency) {
                rigThread->setFrequency(frequency);
            }
        }
#endif
    }
}

double SoapyAudio::getFrequency(const int direction, const size_t channel, const std::string &name) const
{
    if (name == "RF")
    {
#ifdef USE_HAMLIB
        if (rigThread && !rigThread->isTerminated()) {
            return rigThread->getFrequency();
        }
#endif
        return (double) centerFrequency;
    }

    return 0;
}

std::vector<std::string> SoapyAudio::listFrequencies(const int direction, const size_t channel) const
{
    std::vector<std::string> names;
    names.push_back("RF");
    return names;
}

SoapySDR::RangeList SoapyAudio::getFrequencyRange(
        const int direction,
        const size_t channel,
        const std::string &name) const
{
    SoapySDR::RangeList results;
    if (name == "RF")
    {
        results.push_back(SoapySDR::Range(0, 6000000000));
    }
    return results;
}

SoapySDR::ArgInfoList SoapyAudio::getFrequencyArgsInfo(const int direction, const size_t channel) const
{
    SoapySDR::ArgInfoList freqArgs;

    // TODO: frequency arguments

    return freqArgs;
}

/*******************************************************************
 * Sample Rate API
 ******************************************************************/

void SoapyAudio::setSampleRate(const int direction, const size_t channel, const double rate)
{
    SoapySDR_logf(SOAPY_SDR_DEBUG, "Setting sample rate: %d", sampleRate);

    if (sampleRate != rate) {
        sampleRate = rate;
        resetBuffer = true;
        sampleRateChanged.store(true);
    }
}

double SoapyAudio::getSampleRate(const int direction, const size_t channel) const
{
    return sampleRate;
}

std::vector<double> SoapyAudio::listSampleRates(const int direction, const size_t channel) const
{
    std::vector<double> results;

    RtAudio endac;
    RtAudio::DeviceInfo info = endac.getDeviceInfo(deviceId);

    std::vector<unsigned int>::iterator srate;

    for (srate = info.sampleRates.begin(); srate != info.sampleRates.end(); srate++) {
        results.push_back(*srate);
    }

    return results;
}

void SoapyAudio::setBandwidth(const int direction, const size_t channel, const double bw)
{
    SoapySDR::Device::setBandwidth(direction, channel, bw);
}

double SoapyAudio::getBandwidth(const int direction, const size_t channel) const
{
    return SoapySDR::Device::getBandwidth(direction, channel);
}

std::vector<double> SoapyAudio::listBandwidths(const int direction, const size_t channel) const
{
    std::vector<double> results;

    return results;
}

/*******************************************************************
 * Settings API
 ******************************************************************/

SoapySDR::ArgInfoList SoapyAudio::getSettingInfo(void) const
{
    SoapySDR::ArgInfoList setArgs;

#ifdef USE_HAMLIB
    // Rig Control
    SoapySDR::ArgInfo rigArg;
    rigArg.key = "rig";
    rigArg.value = "";
    rigArg.name = "Rig Control";
    rigArg.description = "Select hamlib rig control type.";
    rigArg.type = SoapySDR::ArgInfo::STRING;
    
    std::vector<std::string> rigOpts;
    std::vector<std::string> rigOptNames;

    rigOpts.push_back("");
    rigOptNames.push_back("None");
    
    for (std::vector<const struct rig_caps *>::const_iterator i = rigCaps.begin(); i != rigCaps.end(); i++) {
        const struct rig_caps *rc = (*i);

        rigOpts.push_back(std::to_string(rc->rig_model));
        rigOptNames.push_back(std::string(rc->mfg_name) + " " + std::string(rc->model_name));
    }

    rigArg.options = rigOpts;
    rigArg.optionNames = rigOptNames;

    setArgs.push_back(rigArg);

    // Rig Control
    SoapySDR::ArgInfo rigRateArg;
    rigRateArg.key = "rig_rate";
    rigRateArg.value = "57600";
    rigRateArg.name = "Rig Serial Rate";
    rigRateArg.description = "Select hamlib rig serial control rate.";
    rigRateArg.type = SoapySDR::ArgInfo::STRING;
    
    std::vector<std::string> rigRateOpts;
    std::vector<std::string> rigRateOptNames;

    rigRateOpts.push_back("1200");
    rigRateOptNames.push_back("1200 baud");
    rigRateOpts.push_back("2400");
    rigRateOptNames.push_back("2400 baud");
    rigRateOpts.push_back("4800");
    rigRateOptNames.push_back("4800 baud");
    rigRateOpts.push_back("9600");
    rigRateOptNames.push_back("9600 baud");
    rigRateOpts.push_back("19200");
    rigRateOptNames.push_back("19200 baud");
    rigRateOpts.push_back("38400");
    rigRateOptNames.push_back("38400 baud");
    rigRateOpts.push_back("57600");
    rigRateOptNames.push_back("57600 baud");
    rigRateOpts.push_back("115200");
    rigRateOptNames.push_back("115200 baud");
    rigRateOpts.push_back("128000");
    rigRateOptNames.push_back("128000 baud");
    rigRateOpts.push_back("256000");
    rigRateOptNames.push_back("256000 baud");

    rigRateArg.options = rigRateOpts;
    rigRateArg.optionNames = rigRateOptNames;

    setArgs.push_back(rigRateArg);

    SoapySDR::ArgInfo rigFileArg;
    rigFileArg.key = "rig_port";
    rigFileArg.value = "/dev/ttyUSB0";
    rigFileArg.name = "Rig Serial Port";
    rigFileArg.description = "hamlib rig Serial Port dev / COMx / IP-Address";
    rigFileArg.type = SoapySDR::ArgInfo::STRING;
    
    setArgs.push_back(rigFileArg);
    
#endif
    
    return setArgs;
}

void SoapyAudio::writeSetting(const std::string &key, const std::string &value)
{
#ifdef USE_HAMLIB   
    bool rigReset = false; 
    if (key == "rig")
    {
        try {
            rig_model_t newModel = std::stoi(value);
            if (newModel != rigModel) {
                rigReset = true;
                rigModel = newModel;
            }
        } catch (const std::invalid_argument &) {
            rigModel = 0;
        }
    }

    if (key == "rig_rate")
    {
        try {
            int newSerialRate = std::stoi(value);
            if (newSerialRate != rigSerialRate) {
                rigSerialRate = newSerialRate;
                rigReset = true;
            }
        } catch (const std::invalid_argument &) {
            rigSerialRate = 57600;
        }
    }

    if (key == "rig_port")
    {
        if (rigFile != value) {
            rigFile = value;
            rigReset = true;
        }
    }
    
    if (rigReset) {
        if (rigThread && !rigThread->isTerminated()) {
            rigThread->terminate();
        }
        checkRigThread();        
    }
#endif
}

std::string SoapyAudio::readSetting(const std::string &key) const
{
#ifdef USE_HAMLIB
    if (key == "rig")
    {
        return std::to_string(rigModel);
    }
    if (key == "rig_rate")
    {
        return std::to_string(rigSerialRate);
    }
    if (key == "rig_port")
    {
        return rigFile;
    }
#endif
    // SoapySDR_logf(SOAPY_SDR_WARNING, "Unknown setting '%s'", key.c_str());
    return "";
}


chanSetup SoapyAudio::chanSetupStrToEnum(std::string chanOpt) {
    if (chanOpt == "mono_l") {
        return FORMAT_MONO_L;
    } else if (chanOpt == "mono_r") {
        return FORMAT_MONO_R;
    } else if (chanOpt == "stereo_iq") {
        return FORMAT_STEREO_IQ;
    } else if (chanOpt == "stereo_qi") {
        return FORMAT_STEREO_QI;
    } else {
        return FORMAT_MONO_L;
    }
}

#ifdef USE_HAMLIB
void SoapyAudio::checkRigThread() {    
    if (!rigModel || !rigSerialRate || rigFile == "") {
        return;
    }
    if (!rigThread) {
        rigThread = new RigThread();
    }
    if (rigThread->isTerminated()) {
        if (t_Rig && t_Rig->joinable()) {
            t_Rig->join();
            delete t_Rig;
        }
        rigThread->setup(rigModel, rigFile, rigSerialRate);
        t_Rig = new std::thread(&RigThread::threadMain, rigThread);
    }
}

#endif