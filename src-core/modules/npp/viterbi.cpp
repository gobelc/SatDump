#include "viterbi.h"

#define ST_IDLE 0
#define ST_SYNCING 1
#define ST_SYNCED 2

#define ST_PHASE_0 0
#define ST_PHASE_1 1
#define ST_PHASE_2 2
#define ST_PHASE_3 3
#define ST_PHASE_4 4
#define ST_PHASE_5 5
#define ST_PHASE_6 6
#define ST_PHASE_7 7

namespace npp
{
    HRDViterbi::HRDViterbi(bool sync_check, float ber_threshold, int insync_after, int outsync_after, int reset_after, int buffer_size)
        : d_sync_check(sync_check),
          d_insync_after(insync_after),
          d_outsync_after(outsync_after),
          d_reset_after(reset_after),
          d_ber_threshold(ber_threshold)
    {
        insymbols_interleaved_depunctured = new unsigned char[buffer_size * 4];
        decoded_data = new unsigned char[buffer_size];
        encoded_data = new unsigned char[buffer_size * 2];

        input_symbols_buffer_I = new unsigned char[buffer_size];
        input_symbols_buffer_Q = new unsigned char[buffer_size];
        input_symbols_buffer_I_ph = new unsigned char[buffer_size];
        input_symbols_buffer_Q_ph = new unsigned char[buffer_size];

        float RATE = 1 / 2; //0.5
        float ebn0 = 12;    //12
        float esn0 = RATE * pow(10.0, ebn0 / 10);
        gen_met(d_mettab, 100, esn0, 0.0, 256);

        do_reset();
        enter_idle();

        switchInv = false;
    }

    /*
     * Our virtual destructor.
     */
    HRDViterbi::~HRDViterbi()
    {
        delete[] insymbols_interleaved_depunctured;
        delete[] decoded_data;
        delete[] encoded_data;
        delete[] input_symbols_buffer_I_ph;
        delete[] input_symbols_buffer_Q_ph;
        delete[] input_symbols_buffer_I;
        delete[] input_symbols_buffer_Q;
    }

    //*****************************************************************************
    //    DO DECODER RESET TO ZERO STATE
    //*****************************************************************************
    void HRDViterbi::do_reset()
    {
        d_valid_packet_count = 0;
        d_invalid_packet_count = 0;
        //d_chan_len = TestBitsLen;

        viterbi_chunks_init(d_state0); //main viterbi decoder state memory
        viterbi_chunks_init(d_00_st0);
        viterbi_chunks_init(d_180_st0);
        enter_idle();
    }
    //#############################################################################

    //*****************************************************************************
    //    ENTER idle state
    //*****************************************************************************
    void HRDViterbi::enter_idle()
    {
        d_state = ST_IDLE;
        d_valid_packet_count = 0;
        d_shift = 0;
        d_curr_is_even = true;
        d_bits = 0;
        d_sym_count = 0;
        d_valid_ber_found = true;
        d_viterbi_enable = false;
        d_invalid_packet_count = 0;
        d_shift_main_decoder = 0;
    }
    //#############################################################################

    //*****************************************************************************
    //    ENTER synced state
    //*****************************************************************************
    void HRDViterbi::enter_synced()
    {
        d_state = ST_SYNCED;
        d_invalid_packet_count = 0;
        d_viterbi_enable = true;
    }

    //*****************************************************************************
    //    VITERBI DECODER, calculate BER between hard input bits and decode-encoded bits
    //*****************************************************************************
    float HRDViterbi::ber_calc1(
        struct viterbi_state *state0, //state 0 viterbi decoder
        struct viterbi_state *state1, //state 1 viterbi decoder
        unsigned int symsnr,
        unsigned char *insymbols_I, unsigned char *insymbols_Q)
    {

        unsigned char viterbi_in[4];
        unsigned int decoded_data_count = 0;
        unsigned char *p_decoded_data = &decoded_data[0]; //pointer to viterbi decoded data
        unsigned int difference_count;                    //count of diff. between reencoded data and input symbols
        float ber;
        unsigned char symbol_count = 0;
        unsigned int bits = 0;

        //decode test packet of incoming symbols
        //depuncturing is included here
        for (unsigned int i = 0; i < symsnr; i++)
        {
            viterbi_in[bits % 4] = switchInv ? -insymbols_I[i] : insymbols_I[i];
            insymbols_interleaved_depunctured[bits] = switchInv ? -insymbols_Q[i] : insymbols_I[i];
            bits++;
            viterbi_in[bits % 4] = switchInv ? insymbols_Q[i] : -insymbols_Q[i];
            insymbols_interleaved_depunctured[bits] = switchInv ? insymbols_I[i] : -insymbols_Q[i];

            if ((bits % 4) == 3)
            {
                // Every fourth symbol, perform butterfly operation
                viterbi_butterfly2(viterbi_in, d_mettab, state0, state1);
                // Every sixteenth symbol, read out a byte
                if (bits % 16 == 11)
                {
                    viterbi_get_output(state0, p_decoded_data++);
                    decoded_data_count++;
                }
            }
            bits++;
            symbol_count++;
        }

        //now we have decoded and we will reencode and compare difference between input symbols and reencoded data
        encode(&encoded_data[0], &decoded_data[1], decoded_data_count - 1, 0);

        // compare
        difference_count = 0;
        bits = 0;
        for (unsigned int k = 0; k < decoded_data_count; k++)
        {
            difference_count += ((insymbols_interleaved_depunctured[k] > 128) != (encoded_data[k]));
            bits++;
        }

        //calculate BER

        ber = float(difference_count) / float(bits);

        return ber;
    }

    //*****************************************************************************
    //    VITERBI DECODER, two states symbols phase moving, 0 and 90 degree
    //*****************************************************************************
    void HRDViterbi::phase_move_two(unsigned char phase_state, unsigned int symsnr, unsigned char *in_I, unsigned char *in_Q, unsigned char *out_I, unsigned char *out_Q)
    {
        switch (phase_state)
        {

        case ST_PHASE_0: //nothing is changed
            for (unsigned int ii = 0; ii < symsnr; ii++)
            {
                out_I[ii] = in_I[ii];
                out_Q[ii] = in_Q[ii];
            }
            break;

        case ST_PHASE_1: // rotate 90degree
            for (unsigned int ii = 0; ii < symsnr; ii++)
            {
                out_I[ii] = in_Q[ii];
                out_Q[ii] = ~in_I[ii]; //out_Q[ii] = -in_I[ii];
            }
            break;

        default:
            throw std::runtime_error("Viterbi decoder: bad phase state\n");
        }
    }

    //*****************************************************************************
    //    VITERBI DECODER, GENERAL WORK FUNCTION
    //
    //*****************************************************************************
    int HRDViterbi::work(std::complex<float> *in_syms, int size, uint8_t *output)
    {
        unsigned char *out = &output[0];
        int ninputs = size;

        //translate all complex insymbols to char and save these to input_symbols_buffer's I and Q
        float sample;
        for (int i = 0; i < ninputs; i++)
        {
            // Translate and clip [-1.0..1.0] to [28..228]
            sample = in_syms[i].real() * 127.0 + 128.0;
            if (sample > 255.0)
                sample = 255.0;
            else if (sample < 0.0)
                sample = 0.0;
            input_symbols_buffer_I[i] = (unsigned char)(floor(sample));

            sample = in_syms[i].imag() * 127.0 + 128.0;
            if (sample > 255.0)
                sample = 255.0;
            else if (sample < 0.0)
                sample = 0.0;
            input_symbols_buffer_Q[i] = (unsigned char)(floor(sample));
        }

        //check data chunk, even or odd syms count
        if (ninputs % 2 == 0)
        {
            d_curr_is_even = true; //first bit in next processed input syms paket will be even.
        }
        else
        {
            d_curr_is_even = false;
        }

        switch (d_state)
        {
        //ST_IDLE is waiting for valid BER measured on incoming data
        case ST_IDLE:
            //first check BER of NO SHIFTed data for 0 and 90 degree rotation
            d_valid_ber_found = true;

            for (unsigned char st = 0; st < 2; st++)
            {
                phase_move_two(st, TestBitsLen, input_symbols_buffer_I, input_symbols_buffer_Q, input_symbols_buffer_I_ph, input_symbols_buffer_Q_ph);
                d_ber[0][st] = ber_calc1(d_00_st0, d_00_st1, TestBitsLen, input_symbols_buffer_I_ph, input_symbols_buffer_Q_ph);
                //printf("Viterbi decoder :noshift  PH%i:  d_ber %4f  \n", st, d_ber[0][st]);
            }

            if (d_ber[0][0] < d_ber_threshold)
            {
                d_phase = 0;
                d_shift = 0;
            }
            else if (d_ber[0][1] < d_ber_threshold)
            {
                d_phase = 1;
                d_shift = 0;
            }
            else
            {
                //second check BER of NO SHIFTed data for 0 and 90 degree rotation
                for (unsigned char st = 0; st < 2; st++)
                {
                    phase_move_two(st, TestBitsLen, input_symbols_buffer_I, input_symbols_buffer_Q, input_symbols_buffer_I_ph, input_symbols_buffer_Q_ph);
                    d_ber[1][st] = ber_calc1(d_00_st0, d_00_st1, TestBitsLen, input_symbols_buffer_I_ph + 1, input_symbols_buffer_Q_ph + 1);
                    //printf("Viterbi decoder : shifted PH%i:  d_ber %4f  \n", st, d_ber[1][st]);
                }
                if (d_ber[1][0] < d_ber_threshold)
                {
                    d_shift = 1;
                    d_phase = 0;
                }
                else if (d_ber[1][1] < d_ber_threshold)
                {
                    d_phase = 1;
                    d_shift = 1;
                }
                //all ber >> threshold, wait for next data chunk
                else
                {
                    d_valid_ber_found = false;
                    //printf("Viterbi decoder : ST_IDLE: NO VALID BER found,  waiting for next  packet of symbols\n");
                }
            }

            if (d_valid_ber_found == true)
            {
                enter_synced();
                if (d_shift == 0)
                {
                    if (d_curr_is_even == false)
                    {
                        d_shift_main_decoder = 1;
                    }
                    else
                    {
                        d_shift_main_decoder = 0;
                    }
                }
                else
                {
                    if (d_curr_is_even == false)
                    {
                        d_shift_main_decoder = 0;
                    }
                    else
                    {
                        d_shift_main_decoder = 1;
                    }
                }
            }

            break;

        //ST_SYNCED check BER on incoming data if eneble, activate main decoder decode all incoming data
        case ST_SYNCED:

            if (d_shift == 0)
            {
                phase_move_two(d_phase, TestBitsLen, input_symbols_buffer_I, input_symbols_buffer_Q, input_symbols_buffer_I_ph, input_symbols_buffer_Q_ph);
                d_ber[0][0] = ber_calc1(d_00_st0, d_00_st1, TestBitsLen, input_symbols_buffer_I_ph, input_symbols_buffer_Q_ph);
            }
            else
            {
                phase_move_two(d_phase, TestBitsLen, input_symbols_buffer_I, input_symbols_buffer_Q, input_symbols_buffer_I_ph, input_symbols_buffer_Q_ph);
                d_ber[0][0] = ber_calc1(d_00_st0, d_00_st1, TestBitsLen, input_symbols_buffer_I_ph + 1, input_symbols_buffer_Q_ph + 1);
            }

            if (d_ber[0][0] > d_ber_threshold)
            {
                d_invalid_packet_count++;
                //printf("Viterbi decoder : ST_SYNCED: Chunk Nr %i BER = %4f and exceed d_ber_threshold = %4f \n", d_invalid_packet_count, d_ber[0][0], d_ber_threshold);
                if (d_invalid_packet_count > d_outsync_after)
                {
                    //printf("Viterbi decoder : ST_SYNCED: switch to ST_IDLE >> enter_idle()\n");
                    enter_idle();
                }
            }
            else
            {
                d_invalid_packet_count = 0;
                d_viterbi_enable = true; //!!!
            }

            break;

        default:
            throw std::runtime_error("Viterbi decoder: bad state\n");
        }

        //is this data chunk even or odd? determine if shift in next chunk will be apply
        if (d_shift == 0)
        {
            if (d_curr_is_even == false)
            { //lichy
                d_shift = 1;
            }
            else
            { //sudy
                d_shift = 0;
            }
        }
        else
        {
            if (d_curr_is_even == false)
            { //lichy
                d_shift = 0;
            }
            else
            { //sudy
                d_shift = 1;
            }
        }

        //****************************
        //from here start main decoder
        //****************************
        // depuncturing is included
        if (d_viterbi_enable == true)
        {
            phase_move_two(d_phase, ninputs, input_symbols_buffer_I, input_symbols_buffer_Q, input_symbols_buffer_I_ph, input_symbols_buffer_Q_ph);

            unsigned int out_byte_count = 0;

            for (int i = d_shift_main_decoder; i < ninputs; i++)
            {
                d_even_symbol = true;
                d_viterbi_in[d_bits % 4] = switchInv ? -input_symbols_buffer_Q_ph[i] : input_symbols_buffer_I_ph[i];
                d_bits++;
                d_viterbi_in[d_bits % 4] = switchInv ? input_symbols_buffer_I_ph[i] : -input_symbols_buffer_Q_ph[i];
                if ((d_bits % 4) == 3)
                {
                    // Every fourth symbol, perform butterfly operation
                    viterbi_butterfly2(d_viterbi_in, d_mettab, d_state0, d_state1);
                    // Every sixteenth symbol, read out a byte
                    if (d_bits % 16 == 11)
                    {
                        viterbi_get_output(d_state0, out++);
                        out_byte_count++;
                    }
                }
                d_bits++;
                d_sym_count++;
            }
            d_shift_main_decoder = 0; //no shift next time

            if (d_sym_count % 2 == 0)
            {
                d_even_symbol = true; //first bit in next processed input syms paket will be even.
            }
            else
            {
                d_even_symbol = false;
            }

            //consume_each(ninputs);
            return (out_byte_count);
        }
        else
        {
            //consume_each(ninputs);
            return (0);
        }
    }

    unsigned char &HRDViterbi::getState()
    {
        return d_state;
    }

    float HRDViterbi::ber()
    {
        if (d_state == ST_SYNCED)
            return d_ber[0][0];
        else
        {
            float ber = 10;
            for (int s = 0; s < 2; s++)
            {
                for (int p = 0; p < 2; p++)
                {
                    if (ber > d_ber[s][p])
                    {
                        ber = d_ber[s][p];
                    }
                }
            }
            return ber;
        }
    }
} // namespace npp