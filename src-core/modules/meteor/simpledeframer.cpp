#include "simpledeframer.h"

#include <math.h>

// Returns the asked bit!
template <typename T>
inline bool getBit(T &data, int &bit)
{
    return (data >> bit) & 1;
}

template <typename SYNC_T, int SYNC_SIZE, int FRAME_SIZE, SYNC_T ASM_SYNC, int chk_size>
SimpleDeframer<SYNC_T, SYNC_SIZE, FRAME_SIZE, ASM_SYNC, chk_size>::SimpleDeframer()
{
    // Default values
    writeFrame = false;
    wroteBits = 0;
    outputBits = 0;
}

// Write a single bit into the frame
template <typename SYNC_T, int SYNC_SIZE, int FRAME_SIZE, SYNC_T ASM_SYNC, int chk_size>
void SimpleDeframer<SYNC_T, SYNC_SIZE, FRAME_SIZE, ASM_SYNC, chk_size>::pushBit(uint8_t bit)
{
    byteBuffer = (byteBuffer << 1) | bit;
    wroteBits++;
    if (wroteBits == 8)
    {
        frameBuffer.push_back(byteBuffer);
        wroteBits = 0;
    }
}

// Compare 2 32-bits values bit per bit
template <typename T>
int checkSyncMarker(T marker, T &totest, int size, int max)
{
    int errors = 0;
    for (int i = size; i >= 0; i--)
    {
        bool markerBit, testBit;
        markerBit = getBit<T>(marker, i);
        testBit = getBit<T>(totest, i);
        if (markerBit != testBit)
            errors++;

        if (errors >= max)
            return errors;
    }
    return errors;
}

template <typename SYNC_T, int SYNC_SIZE, int FRAME_SIZE, SYNC_T ASM_SYNC, int chk_size>
std::vector<std::vector<uint8_t>> SimpleDeframer<SYNC_T, SYNC_SIZE, FRAME_SIZE, ASM_SYNC, chk_size>::work(std::vector<uint8_t> &data)
{
    // Output buffer
    std::vector<std::vector<uint8_t>> framesOut;

    // Loop in all bytes
    for (uint8_t &byte : data)
    {
        // Loop in all bits!
        for (int i = 7; i >= 0; i--)
        {
            // Get a bit, push it
            uint8_t bit = getBit<uint8_t>(byte, i);

            if (sizeof(SYNC_T) * 8 != SYNC_SIZE)
                shifter = ((shifter << 1) % (long)pow(2, SYNC_SIZE)) | bit;
            else
                shifter = (shifter << 1) | bit;

            // Writing a frame!
            if (writeFrame)
            {
                // First run : push header
                if (outputBits == 0)
                {
                    SYNC_T syncAsm = ASM_SYNC;
                    for (int y = SYNC_SIZE - 1; y >= 0; y--)
                    {
                        pushBit(getBit<SYNC_T>(syncAsm, y));
                        outputBits++;
                    }
                }

                // New ASM, ABORT! and process the new one
                if (chk_size == 0 ? shifter == ASM_SYNC : checkSyncMarker<SYNC_T>(ASM_SYNC, shifter, SYNC_SIZE - 1, chk_size) < chk_size)
                {
                    // Fill up what we're missing
                    for (int b = 0; b < FRAME_SIZE - outputBits; b++)
                        pushBit(0);

                    writeFrame = false;
                    wroteBits = 0;
                    outputBits = 0;
                    framesOut.push_back(frameBuffer);
                    frameBuffer.clear();

                    writeFrame = true;

                    continue;
                }

                // Push current bit
                pushBit(bit);
                outputBits++;

                // Once we wrote a frame, exit!
                if (outputBits == FRAME_SIZE)
                {
                    writeFrame = false;
                    wroteBits = 0;
                    outputBits = 0;
                    framesOut.push_back(frameBuffer);
                    frameBuffer.clear();
                }

                continue;
            }

            // Otherwise search for markers
            if (chk_size == 0 ? shifter == ASM_SYNC : checkSyncMarker<SYNC_T>(ASM_SYNC, shifter, SYNC_SIZE - 1, chk_size) < chk_size)
            {
                writeFrame = true;
            }
        }
    }

    // Output what we found if anything
    return framesOut;
}

// Build this template for MSU-MR data
template class SimpleDeframer<uint64_t, 64, 11850 * 8, 0x0218A7A392DD9ABF, 10>;
// Build this template for TLM data
template class SimpleDeframer<uint64_t, 64, 74 * 8, 0x0218A7A392DD9ABF, 0>;
// Build this template for BIS-M data
template class SimpleDeframer<uint32_t, 32, 88 * 8, 0x71DE2CD8, 0>;
// Build this template for MTVZA data
template class SimpleDeframer<uint32_t, 32, 248 * 8, 0xFB386A45, 0>;
// Build this template for SSPD data
template class SimpleDeframer<uint64_t, 24, 99 * 8, 0x42BB1F, 0>;