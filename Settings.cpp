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
}

SoapyAudio::~SoapyAudio(void)
{
    //cleanup device handles
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
    }
}

double SoapyAudio::getFrequency(const int direction, const size_t channel, const std::string &name) const
{
    if (name == "RF")
    {
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

    return setArgs;
}

void SoapyAudio::writeSetting(const std::string &key, const std::string &value)
{
    // if (key == "")
    // {
    // }
}

std::string SoapyAudio::readSetting(const std::string &key) const
{
    // if (key == "") {
    //     return "";
    // }

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
