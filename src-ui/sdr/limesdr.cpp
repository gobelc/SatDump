#include "limesdr.h"
#include <sstream>
#include "imgui/imgui.h"
#include "logger.h"

#if 0
#ifdef BUILD_LIVE

SDRLimeSDR::SDRLimeSDR(std::map<std::string, std::string> parameters, uint64_t id) : SDRDevice(parameters, id)
{
    READ_PARAMETER_IF_EXISTS_FLOAT(gain_tia, "tia_gain");
    READ_PARAMETER_IF_EXISTS_FLOAT(gain_lna, "lna_gain");
    READ_PARAMETER_IF_EXISTS_FLOAT(gain_pga, "pga_gain");

    limeDevice = lime::LMS7_Device::CreateDevice(lime::ConnectionRegistry::findConnections()[id]);
    logger->info("Opened " + std::string(limeDevice->GetInfo()->deviceName) + " device!");

    limeDevice->Init();
}

void SDRLimeSDR::start()
{
    limeDevice->EnableChannel(false, 0, true);

    limeDevice->SetPath(false, 0, 3);

    logger->info("Samplerate " + std::to_string(d_samplerate));
    limeDevice->SetRate(d_samplerate, 0);
    limeDevice->SetLPF(false, 0, true, d_samplerate);

    logger->info("Frequency " + std::to_string(d_frequency));
    limeDevice->SetFrequency(false, 0, d_frequency);
    limeDevice->SetClockFreq(LMS_CLOCK_SXR, d_frequency, 0);

    limeDevice->SetGain(false, 0, gain_tia, "TIA");
    limeDevice->SetGain(false, 0, gain_lna, "LNA");
    limeDevice->SetGain(false, 0, gain_pga, "PGA");

    limeConfig.align = false;
    limeConfig.isTx = false;
    limeConfig.performanceLatency = 0.5;
    limeConfig.bufferLength = 0; //auto
    limeConfig.format = lime::StreamConfig::FMT_FLOAT32;
    limeConfig.channelID = 0;

    limeStreamID = limeDevice->SetupStream(limeConfig);

    if (limeStreamID == 0)
    {
        logger->error("Could not open LimeSDR device!");
    }

    limeStream = limeStreamID;

    // Start
    should_run = true;
    workThread = std::thread(&SDRLimeSDR::runThread, this);
    limeStream->Start();
}

std::map<std::string, std::string> SDRLimeSDR::getParameters()
{
    d_parameters["tia_gain"] = std::to_string(gain_tia);
    d_parameters["lna_gain"] = std::to_string(gain_lna);
    d_parameters["pga_gain"] = std::to_string(gain_pga);

    return d_parameters;
}

void SDRLimeSDR::runThread()
{
    lime::StreamChannel::Metadata md;

    while (should_run)
    {
        int cnt = limeStream->Read(output_stream->writeBuf, 8192 * 10, &md);
        output_stream->swap(cnt);
    }
}

void SDRLimeSDR::stop()
{
    //airspy_stop_rx(dev);
    should_run = false;
    limeStream->Stop();
    if (workThread.joinable())
        workThread.join();
}

SDRLimeSDR::~SDRLimeSDR()
{
    limeDevice->DestroyStream(limeStream);
}

void SDRLimeSDR::drawUI()
{
    ImGui::Begin("LimeSDR Control", NULL);

    ImGui::SetNextItemWidth(100);
    ImGui::InputText("MHz", frequency, 100);

    ImGui::SameLine();

    if (ImGui::Button("Set"))
    {
        d_frequency = std::stof(frequency) * 1e6;
        limeDevice->SetFrequency(false, 0, d_frequency);
        limeDevice->SetClockFreq(LMS_CLOCK_SXR, d_frequency, 0);
    }

    ImGui::SetNextItemWidth(200);
    if (ImGui::SliderInt("TIA Gain", &gain_tia, 0, 12))
    {
        limeDevice->SetGain(false, 0, gain_tia, "TIA");
    }

    ImGui::SetNextItemWidth(200);
    if (ImGui::SliderInt("LNA Gain", &gain_lna, 0, 30))
    {
        limeDevice->SetGain(false, 0, gain_lna, "LNA");
    }

    ImGui::SetNextItemWidth(200);
    if (ImGui::SliderInt("PGA Gain", &gain_pga, -12, 19))
    {
        limeDevice->SetGain(false, 0, gain_pga, "PGA");
    }

    ImGui::End();
}

void SDRLimeSDR::setFrequency(float frequency)
{
    d_frequency = frequency;
    std::memcpy(this->frequency, std::to_string((float)d_frequency / 1e6).c_str(), std::to_string((float)d_frequency / 1e6).length());
}

void SDRLimeSDR::init()
{
}

std::vector<std::tuple<std::string, sdr_device_type, uint64_t>> SDRLimeSDR::getDevices()
{
    std::vector<std::tuple<std::string, sdr_device_type, uint64_t>> results;

    std::vector<lime::ConnectionHandle> devs = lime::ConnectionRegistry::findConnections();

    for (int i = 0; i < devs.size() - 1; i++)
    {
        results.push_back({"LimeSDR " + devs[i].serial, LIMESDR, i});
    }

    return results;
}

std::string SDRLimeSDR::getID()
{
    return "limesdr";
}

#endif
#endif