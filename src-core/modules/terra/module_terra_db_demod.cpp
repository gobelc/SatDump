#include "module_terra_db_demod.h"
#include "common/dsp/lib/fir_gen.h"
#include "logger.h"
#include "imgui/imgui.h"

// Return filesize
size_t getFilesize(std::string filepath);

namespace terra
{
    TerraDBDemodModule::TerraDBDemodModule(std::string input_file, std::string output_file_hint, std::map<std::string, std::string> parameters) : ProcessingModule(input_file, output_file_hint, parameters),
                                                                                                                                                  d_samplerate(std::stoi(parameters["samplerate"])),
                                                                                                                                                  d_buffer_size(std::stoi(parameters["buffer_size"])),
                                                                                                                                                  d_dc_block(parameters.count("dc_block") > 0 ? std::stoi(parameters["dc_block"]) : 0)
    {
        // Init DSP blocks
        file_source = std::make_shared<dsp::FileSourceBlock>(d_input_file, dsp::BasebandTypeFromString(parameters["baseband_format"]), d_buffer_size);
        agc = std::make_shared<dsp::AGCBlock>(file_source->output_stream, 1e-3, 1.0f, 1.0f, 65536);
        rrc = std::make_shared<dsp::CCFIRBlock>(agc->output_stream, 1, dsp::firgen::root_raised_cosine(1, d_samplerate, 13.125e6 * 2, 0.5f, 31));
        pll = std::make_shared<dsp::CostasLoopBlock>(rrc->output_stream, 0.004, 2);
        rec = std::make_shared<dsp::CCMMClockRecoveryBlock>(pll->output_stream, ((float)d_samplerate / (float)13.125e6) / 2.0f, pow(0.001, 2) / 4.0, 0.5f, 0.001, 0.0001f);

        // Buffers
        sym_buffer = new int8_t[d_buffer_size * 2];
        snr = 0;
    }

    std::vector<ModuleDataType> TerraDBDemodModule::getInputTypes()
    {
        return {DATA_FILE, DATA_STREAM};
    }

    std::vector<ModuleDataType> TerraDBDemodModule::getOutputTypes()
    {
        return {DATA_FILE};
    }

    TerraDBDemodModule::~TerraDBDemodModule()
    {
        delete[] sym_buffer;
    }

    void TerraDBDemodModule::process()
    {
        if (input_data_type == DATA_FILE)
            filesize = file_source->getFilesize();
        else
            filesize = 0;

        //if (input_data_type == DATA_FILE)
        //    data_in = std::ifstream(d_input_file, std::ios::binary);

        data_out = std::ofstream(d_output_file_hint + ".soft", std::ios::binary);
        d_output_files.push_back(d_output_file_hint + ".soft");

        logger->info("Using input baseband " + d_input_file);
        logger->info("Demodulating to " + d_output_file_hint + ".soft");
        logger->info("Buffer size : " + std::to_string(d_buffer_size));

        time_t lastTime = 0;

        // Start
        file_source->start();
        agc->start();
        rrc->start();
        pll->start();
        rec->start();

        int dat_size = 0;
        while (/*input_data_type == DATA_STREAM ? input_active.load() : */ !file_source->eof())
        {
            dat_size = rec->output_stream->read();

            if (dat_size <= 0)
                continue;

            // Estimate SNR, only on part of the samples to limit CPU usage
            snr_estimator.update(rec->output_stream->readBuf, dat_size / 100);
            snr = snr_estimator.snr();

            for (int i = 0; i < dat_size; i++)
            {
                sym_buffer[i] = clamp(rec->output_stream->readBuf[i].real() * 50);
            }

            rec->output_stream->flush();

            data_out.write((char *)sym_buffer, dat_size);

            progress = file_source->getPosition();
            if (time(NULL) % 10 == 0 && lastTime != time(NULL))
            {
                lastTime = time(NULL);
                logger->info("Progress " + std::to_string(round(((float)progress / (float)filesize) * 1000.0f) / 10.0f) + "%, SNR : " + std::to_string(snr) + "dB");
            }
        }

        logger->info("Demodulation finished");

        // Stop
        file_source->stop();
        agc->stop();
        rrc->stop();
        pll->stop();
        rec->stop();

        data_out.close();
    }

    const ImColor colorNosync = ImColor::HSV(0 / 360.0, 1, 1, 1.0);
    const ImColor colorSyncing = ImColor::HSV(39.0 / 360.0, 0.93, 1, 1.0);
    const ImColor colorSynced = ImColor::HSV(113.0 / 360.0, 1, 1, 1.0);

    void TerraDBDemodModule::drawUI(bool window)
    {
        ImGui::Begin("Terra DB Demodulator", NULL, window ? NULL : NOWINDOW_FLAGS);

        ImGui::BeginGroup();
        // Constellation
        {
            ImDrawList *draw_list = ImGui::GetWindowDrawList();
            draw_list->AddRectFilled(ImGui::GetCursorScreenPos(),
                                     ImVec2(ImGui::GetCursorScreenPos().x + 200 * ui_scale, ImGui::GetCursorScreenPos().y + 200 * ui_scale),
                                     ImColor::HSV(0, 0, 0));

            for (int i = 0; i < 2048; i++)
            {
                draw_list->AddCircleFilled(ImVec2(ImGui::GetCursorScreenPos().x + (int)(100 * ui_scale + rec->output_stream->readBuf[i].real() * 50 * ui_scale) % int(200 * ui_scale),
                                                  ImGui::GetCursorScreenPos().y + (int)(100 * ui_scale + rec->output_stream->readBuf[i].imag() * 50 * ui_scale) % int(200 * ui_scale)),
                                           2 * ui_scale,
                                           ImColor::HSV(113.0 / 360.0, 1, 1, 1.0));
            }

            ImGui::Dummy(ImVec2(200 * ui_scale + 3, 200 * ui_scale + 3));
        }
        ImGui::EndGroup();

        ImGui::SameLine();

        ImGui::BeginGroup();
        {
            ImGui::Button("Signal", {200 * ui_scale, 20 * ui_scale});
            {
                ImGui::Text("SNR (dB) : ");
                ImGui::SameLine();
                ImGui::TextColored(snr > 2 ? snr > 10 ? colorSynced : colorSyncing : colorNosync, UITO_C_STR(snr));

                std::memmove(&snr_history[0], &snr_history[1], (200 - 1) * sizeof(float));
                snr_history[200 - 1] = snr;

                ImGui::PlotLines("", snr_history, IM_ARRAYSIZE(snr_history), 0, "", 0.0f, 25.0f, ImVec2(200 * ui_scale, 50 * ui_scale));
            }
        }
        ImGui::EndGroup();

        ImGui::ProgressBar((float)progress / (float)filesize, ImVec2(ImGui::GetWindowWidth() - 10, 20 * ui_scale));

        ImGui::End();
    }

    std::string TerraDBDemodModule::getID()
    {
        return "terra_db_demod";
    }

    std::vector<std::string> TerraDBDemodModule::getParameters()
    {
        return {"samplerate", "buffer_size", "dc_block", "baseband_format"};
    }

    std::shared_ptr<ProcessingModule> TerraDBDemodModule::getInstance(std::string input_file, std::string output_file_hint, std::map<std::string, std::string> parameters)
    {
        return std::make_shared<TerraDBDemodModule>(input_file, output_file_hint, parameters);
    }
}