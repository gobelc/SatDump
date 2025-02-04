/*
 * DifferentialDecoder.h
 *
 *  Created on: 25/01/2017
 *      Author: Lucas Teske
 *      Modified by Aang23
 */
#pragma once

#include <cstdint>

namespace diff
{
    // Countinuous decoder
    class NRZSDiff
    {
    private:
        uint8_t mask;
        uint8_t lastBit = 0;

    public:
        void decode(uint8_t *data, int length);
    };

    // Decode once with no continuity
    void nrzs_decode(uint8_t *data, int length);
} // namespace diff