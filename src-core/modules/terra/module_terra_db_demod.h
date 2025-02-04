#pragma once

#include "module.h"
#include <complex>
#include <thread>
#include <fstream>
#include "common/dsp/agc.h"
#include "common/dsp/fir.h"
#include "common/dsp/costas_loop.h"
#include "common/dsp/clock_recovery_mm.h"
#include "common/dsp/file_source.h"
#include "common/snr_estimator.h"

namespace terra
{
    class TerraDBDemodModule : public ProcessingModule
    {
    protected:
        std::shared_ptr<dsp::FileSourceBlock> file_source;
        std::shared_ptr<dsp::AGCBlock> agc;
        std::shared_ptr<dsp::CCFIRBlock> rrc;
        std::shared_ptr<dsp::CostasLoopBlock> pll;
        std::shared_ptr<dsp::CCMMClockRecoveryBlock> rec;

        const int d_samplerate;
        const int d_buffer_size;
        const bool d_dc_block;

        int8_t *sym_buffer;

        int8_t clamp(float x)
        {
            if (x < -128.0)
                return -127;
            if (x > 127.0)
                return 127;
            return x;
        }

        std::ofstream data_out;

        std::atomic<uint64_t> filesize;
        std::atomic<uint64_t> progress;

        M2M4SNREstimator snr_estimator;
        float snr;

        // UI Stuff
        float snr_history[200];

    public:
        TerraDBDemodModule(std::string input_file, std::string output_file_hint, std::map<std::string, std::string> parameters);
        ~TerraDBDemodModule();
        void process();
        void drawUI(bool window);
        std::vector<ModuleDataType> getInputTypes();
        std::vector<ModuleDataType> getOutputTypes();

    public:
        static std::string getID();
        static std::vector<std::string> getParameters();
        static std::shared_ptr<ProcessingModule> getInstance(std::string input_file, std::string output_file_hint, std::map<std::string, std::string> parameters);
    };
}