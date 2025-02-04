#include "msu_vis_reader.h"

namespace elektro_arktika
{
    namespace msugs
    {
        MSUVISReader::MSUVISReader()
        {
            imageBuffer = new unsigned short[30000 * 12008];
            frames = 0;
        }

        MSUVISReader::~MSUVISReader()
        {
            delete[] imageBuffer;
        }

        void MSUVISReader::pushFrame(uint8_t *data)
        {
            // Offset to start reading from
            int pos = 5 * 38;

            // Convert to 10 bits values
            for (int i = 0; i < 12044; i += 4)
            {
                msuLineBuffer[i] = (data[pos + 0] << 2) | (data[pos + 1] >> 6);
                msuLineBuffer[i + 1] = ((data[pos + 1] % 64) << 4) | (data[pos + 2] >> 4);
                msuLineBuffer[i + 2] = ((data[pos + 2] % 16) << 6) | (data[pos + 3] >> 2);
                msuLineBuffer[i + 3] = ((data[pos + 3] % 4) << 8) | data[pos + 4];
                pos += 5;
            }

            // Deinterleave and load into our image buffer
            for (int i = 0; i < 6004; i++)
            {
                imageBuffer[frames * 12008 + i] = msuLineBuffer[i * 2 + 0] * 60;
                imageBuffer[frames * 12008 + 6000 + i] = msuLineBuffer[i * 2 + 1] * 60;
            }

            frames++;
        }

        cimg_library::CImg<unsigned short> MSUVISReader::getImage()
        {
            return cimg_library::CImg<unsigned short>(&imageBuffer[0], 12008, frames);
        }
    } // namespace msugs
} // namespace elektro_arktika