#include "module_goes_hrit_decoder.h"
#include "logger.h"
#include "common/codings/reedsolomon/reedsolomon.h"
#include "common/codings/differential/nrzm.h"
#include "imgui/imgui.h"

#define BUFFER_SIZE 8192 * 2

// Return filesize
size_t getFilesize(std::string filepath);

namespace goes
{
    namespace hrit
    {
        GOESHRITDecoderModule::GOESHRITDecoderModule(std::string input_file, std::string output_file_hint, std::map<std::string, std::string> parameters) : ProcessingModule(input_file, output_file_hint, parameters),
                                                                                                                                                            d_viterbi_outsync_after(std::stoi(parameters["viterbi_outsync_after"])),
                                                                                                                                                            d_viterbi_ber_threasold(std::stof(parameters["viterbi_ber_thresold"])),
                                                                                                                                                            viterbi(d_viterbi_ber_threasold, d_viterbi_outsync_after, BUFFER_SIZE)
        {
            viterbi_out = new uint8_t[BUFFER_SIZE * 2];
            soft_buffer = new int8_t[BUFFER_SIZE];
        }

        std::vector<ModuleDataType> GOESHRITDecoderModule::getInputTypes()
        {
            return {DATA_FILE, DATA_STREAM};
        }

        std::vector<ModuleDataType> GOESHRITDecoderModule::getOutputTypes()
        {
            return {DATA_FILE, DATA_STREAM};
        }

        GOESHRITDecoderModule::~GOESHRITDecoderModule()
        {
            delete[] viterbi_out;
            delete[] soft_buffer;
        }

        void GOESHRITDecoderModule::process()
        {
            if (input_data_type == DATA_FILE)
                filesize = getFilesize(d_input_file);
            else
                filesize = 0;
            if (input_data_type == DATA_FILE)
                data_in = std::ifstream(d_input_file, std::ios::binary);
            if (output_data_type == DATA_FILE)
            {
                data_out = std::ofstream(d_output_file_hint + ".cadu", std::ios::binary);
                d_output_files.push_back(d_output_file_hint + ".cadu");
            }

            logger->info("Using input symbols " + d_input_file);
            logger->info("Decoding to " + d_output_file_hint + ".cadu");

            time_t lastTime = 0;

            diff::NRZMDiff diff;

            reedsolomon::ReedSolomon rs(reedsolomon::RS223);

            int vout;

            while (input_data_type == DATA_FILE ? !data_in.eof() : input_active.load())
            {
                // Read a buffer
                if (input_data_type == DATA_FILE)
                    data_in.read((char *)soft_buffer, BUFFER_SIZE);
                else
                    input_fifo->read((uint8_t *)soft_buffer, BUFFER_SIZE);

                // Perform Viterbi decoding
                vout = viterbi.work((uint8_t *)soft_buffer, BUFFER_SIZE, viterbi_out);

                // Perform differential decoding
                diff.decode(viterbi_out, vout);

                if (vout > 0)
                {
                    // Deframe (and derand)
                    std::vector<std::array<uint8_t, ccsds::ccsds_1_0_1024::CADU_SIZE>> frames = deframer.work(viterbi_out, vout);

                    // if we found frame, write them
                    if (frames.size() > 0)
                    {
                        for (std::array<uint8_t, ccsds::ccsds_1_0_1024::CADU_SIZE> cadu : frames)
                        {
                            // RS Correction
                            rs.decode_interlaved(&cadu[4], true, 4, errors);

                            // Write it out
                            if (output_data_type == DATA_FILE)
                                data_out.write((char *)&cadu, ccsds::ccsds_1_0_1024::CADU_SIZE);
                            else
                                output_fifo->write((uint8_t *)&cadu, ccsds::ccsds_1_0_1024::CADU_SIZE);
                        }
                    }
                }

                if (input_data_type == DATA_FILE)
                    progress = data_in.tellg();

                if (time(NULL) % 10 == 0 && lastTime != time(NULL))
                {
                    lastTime = time(NULL);
                    std::string viterbi_state = viterbi.getState() == 0 ? "NOSYNC" : "SYNCED";
                    std::string deframer_state = deframer.getState() == 0 ? "NOSYNC" : (deframer.getState() == 2 || deframer.getState() == 6 ? "SYNCING" : "SYNCED");
                    logger->info("Progress " + std::to_string(round(((float)progress / (float)filesize) * 1000.0f) / 10.0f) + "%, Viterbi : " + viterbi_state + " BER : " + std::to_string(viterbi.ber()) + ", Deframer : " + deframer_state);
                }
            }

            if (output_data_type == DATA_FILE)
                data_out.close();
            if (input_data_type == DATA_FILE)
                data_in.close();
        }

        const ImColor colorNosync = ImColor::HSV(0 / 360.0, 1, 1, 1.0);
        const ImColor colorSyncing = ImColor::HSV(39.0 / 360.0, 0.93, 1, 1.0);
        const ImColor colorSynced = ImColor::HSV(113.0 / 360.0, 1, 1, 1.0);

        void GOESHRITDecoderModule::drawUI(bool window)
        {
            ImGui::Begin("GOES HRIT Decoder", NULL, window ? NULL : NOWINDOW_FLAGS);
            float ber = viterbi.ber();

            ImGui::BeginGroup();
            {
                // Constellation
                {
                    ImDrawList *draw_list = ImGui::GetWindowDrawList();
                    draw_list->AddRectFilled(ImGui::GetCursorScreenPos(),
                                             ImVec2(ImGui::GetCursorScreenPos().x + 200 * ui_scale, ImGui::GetCursorScreenPos().y + 200 * ui_scale),
                                             ImColor::HSV(0, 0, 0));

                    for (int i = 0; i < 2048; i++)
                    {
                        draw_list->AddCircleFilled(ImVec2(ImGui::GetCursorScreenPos().x + (int)(100 * ui_scale + (soft_buffer[i] / 127.0) * 130 * ui_scale) % int(200 * ui_scale),
                                                          ImGui::GetCursorScreenPos().y + (int)(100 * ui_scale + rng.gasdev() * 14 * ui_scale) % int(200 * ui_scale)),
                                                   2 * ui_scale,
                                                   ImColor::HSV(113.0 / 360.0, 1, 1, 1.0));
                    }

                    ImGui::Dummy(ImVec2(200 * ui_scale + 3, 200 * ui_scale + 3));
                }
            }
            ImGui::EndGroup();

            ImGui::SameLine();

            ImGui::BeginGroup();
            {
                ImGui::Button("Viterbi", {200 * ui_scale, 20 * ui_scale});
                {
                    ImGui::Text("State : ");

                    ImGui::SameLine();

                    if (viterbi.getState() == 0)
                        ImGui::TextColored(colorNosync, "NOSYNC");
                    else
                        ImGui::TextColored(colorSynced, "SYNCED");

                    ImGui::Text("BER   : ");
                    ImGui::SameLine();
                    ImGui::TextColored(viterbi.getState() == 0 ? colorNosync : colorSynced, UITO_C_STR(ber));

                    std::memmove(&ber_history[0], &ber_history[1], (200 - 1) * sizeof(float));
                    ber_history[200 - 1] = ber;

                    ImGui::PlotLines("", ber_history, IM_ARRAYSIZE(ber_history), 0, "", 0.0f, 1.0f, ImVec2(200 * ui_scale, 50 * ui_scale));
                }

                ImGui::Spacing();

                ImGui::Button("Deframer", {200 * ui_scale, 20 * ui_scale});
                {
                    ImGui::Text("State : ");

                    ImGui::SameLine();

                    if (deframer.getState() == 0)
                        ImGui::TextColored(colorNosync, "NOSYNC");
                    else if (deframer.getState() == 2 || deframer.getState() == 6)
                        ImGui::TextColored(colorSyncing, "SYNCING");
                    else
                        ImGui::TextColored(colorSynced, "SYNCED");
                }

                ImGui::Spacing();

                ImGui::Button("Reed-Solomon", {200 * ui_scale, 20 * ui_scale});
                {
                    ImGui::Text("RS    : ");
                    for (int i = 0; i < 4; i++)
                    {
                        ImGui::SameLine();

                        if (errors[i] == -1)
                            ImGui::TextColored(colorNosync, "%i ", i);
                        else if (errors[i] > 0)
                            ImGui::TextColored(colorSyncing, "%i ", i);
                        else
                            ImGui::TextColored(colorSynced, "%i ", i);
                    }
                }
            }
            ImGui::EndGroup();

            ImGui::ProgressBar((float)progress / (float)filesize, ImVec2(ImGui::GetWindowWidth() - 10, 20 * ui_scale));

            ImGui::End();
        }

        std::string GOESHRITDecoderModule::getID()
        {
            return "goes_hrit_decoder";
        }

        std::vector<std::string> GOESHRITDecoderModule::getParameters()
        {
            return {"viterbi_outsync_after", "viterbi_ber_thresold"};
        }

        std::shared_ptr<ProcessingModule> GOESHRITDecoderModule::getInstance(std::string input_file, std::string output_file_hint, std::map<std::string, std::string> parameters)
        {
            return std::make_shared<GOESHRITDecoderModule>(input_file, output_file_hint, parameters);
        }
    } // namespace npp
}