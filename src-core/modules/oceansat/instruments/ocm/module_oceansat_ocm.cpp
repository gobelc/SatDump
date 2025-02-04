#include "module_oceansat_ocm.h"
#include <fstream>
#include "ocm_reader.h"
#include "logger.h"
#include <filesystem>
#include "imgui/imgui.h"

// Return filesize
size_t getFilesize(std::string filepath);

namespace oceansat
{
    namespace ocm
    {
        OceansatOCMDecoderModule::OceansatOCMDecoderModule(std::string input_file, std::string output_file_hint, std::map<std::string, std::string> parameters) : ProcessingModule(input_file, output_file_hint, parameters)
        {
        }

        void OceansatOCMDecoderModule::process()
        {
            filesize = getFilesize(d_input_file);
            std::ifstream data_in(d_input_file, std::ios::binary);

            std::string directory = d_output_file_hint.substr(0, d_output_file_hint.rfind('/')) + "/OCM";

            logger->info("Using input frames " + d_input_file);
            logger->info("Decoding to " + directory);

            time_t lastTime = 0;

            OCMReader reader;

            uint8_t buffer[92220];

            logger->info("Processing...");

            while (!data_in.eof())
            {
                // Read buffer
                data_in.read((char *)buffer, 92220);

                reader.work(buffer);

                progress = data_in.tellg();

                if (time(NULL) % 10 == 0 && lastTime != time(NULL))
                {
                    lastTime = time(NULL);
                    logger->info("Progress " + std::to_string(round(((float)progress / (float)filesize) * 1000.0f) / 10.0f) + "%");
                }
            }

            data_in.close();

            logger->info("OCM Lines            : " + std::to_string(reader.lines));

            logger->info("Writing images.... (Can take a while)");

            if (!std::filesystem::exists(directory))
                std::filesystem::create_directory(directory);

            cimg_library::CImg<unsigned short> image1 = reader.getChannel(0);
            cimg_library::CImg<unsigned short> image2 = reader.getChannel(1);
            cimg_library::CImg<unsigned short> image3 = reader.getChannel(2);
            cimg_library::CImg<unsigned short> image4 = reader.getChannel(3);
            cimg_library::CImg<unsigned short> image5 = reader.getChannel(4);
            cimg_library::CImg<unsigned short> image6 = reader.getChannel(5);
            cimg_library::CImg<unsigned short> image7 = reader.getChannel(6);
            cimg_library::CImg<unsigned short> image8 = reader.getChannel(7);

            logger->info("Channel 1...");
            WRITE_IMAGE(image1, directory + "/OCM-1.png");

            logger->info("Channel 2...");
            WRITE_IMAGE(image2, directory + "/OCM-2.png");

            logger->info("Channel 3...");
            WRITE_IMAGE(image3, directory + "/OCM-3.png");

            logger->info("Channel 4...");
            WRITE_IMAGE(image4, directory + "/OCM-4.png");

            logger->info("Channel 5...");
            WRITE_IMAGE(image5, directory + "/OCM-5.png");

            logger->info("Channel 6...");
            WRITE_IMAGE(image6, directory + "/OCM-6.png");

            logger->info("Channel 7...");
            WRITE_IMAGE(image7, directory + "/OCM-7.png");

            logger->info("Channel 8...");
            WRITE_IMAGE(image8, directory + "/OCM-8.png");

            logger->info("642 Composite...");
            {
                cimg_library::CImg<unsigned short> image642(4072, reader.lines, 1, 3);
                {
                    image642.draw_image(0, 0, 0, 0, image6);
                    image642.draw_image(0, 0, 0, 1, image4);
                    image642.draw_image(0, 0, 0, 2, image2);
                }
                WRITE_IMAGE(image642, directory + "/OCM-RGB-642.png");
                image642.equalize(1000);
                image642.normalize(0, std::numeric_limits<unsigned char>::max());
                WRITE_IMAGE(image642, directory + "/OCM-RGB-642-EQU.png");
            }

            logger->info("654 Composite...");
            {
                cimg_library::CImg<unsigned short> image654(4072, reader.lines, 1, 3);
                {
                    image654.draw_image(0, 0, 0, 0, image6);
                    image654.draw_image(0, 0, 0, 1, image5);
                    image654.draw_image(0, 0, 0, 2, image4);
                }
                WRITE_IMAGE(image654, directory + "/OCM-RGB-654.png");
                image654.equalize(1000);
                image654.normalize(0, std::numeric_limits<unsigned char>::max());
                WRITE_IMAGE(image654, directory + "/OCM-RGB-654-EQU.png");
            }

            logger->info("754 Composite...");
            {
                cimg_library::CImg<unsigned short> image754(4072, reader.lines, 1, 3);
                {
                    image754.draw_image(0, 0, 0, 0, image7);
                    image754.draw_image(0, 0, 0, 1, image5);
                    image754.draw_image(0, 0, 0, 2, image4);
                }
                WRITE_IMAGE(image754, directory + "/OCM-RGB-754.png");
                image754.equalize(1000);
                image754.normalize(0, std::numeric_limits<unsigned char>::max());
                WRITE_IMAGE(image754, directory + "/OCM-RGB-754-EQU.png");
            }
        }

        void OceansatOCMDecoderModule::drawUI(bool window)
        {
            ImGui::Begin("Oceansat OCM Decoder", NULL, window ? NULL : NOWINDOW_FLAGS);

            ImGui::ProgressBar((float)progress / (float)filesize, ImVec2(ImGui::GetWindowWidth() - 10, 20 * ui_scale));

            ImGui::End();
        }

        std::string OceansatOCMDecoderModule::getID()
        {
            return "oceansat_ocm";
        }

        std::vector<std::string> OceansatOCMDecoderModule::getParameters()
        {
            return {};
        }

        std::shared_ptr<ProcessingModule> OceansatOCMDecoderModule::getInstance(std::string input_file, std::string output_file_hint, std::map<std::string, std::string> parameters)
        {
            return std::make_shared<OceansatOCMDecoderModule>(input_file, output_file_hint, parameters);
        }
    } // namespace avhrr
} // namespace noaa