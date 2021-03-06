#include "coding.h"


/* PS-ADPCM table, defined as rational numbers (as in the spec) */
static const double ps_adpcm_coefs_f[5][2] = {
        {   0.0        ,   0.0        },
        {  60.0 / 64.0 ,   0.0        },
        { 115.0 / 64.0 , -52.0 / 64.0 },
        {  98.0 / 64.0 , -55.0 / 64.0 },
        { 122.0 / 64.0 , -60.0 / 64.0 },
};

/* PS-ADPCM table, defined as spec_coef*64 (for int implementations) */
static const int ps_adpcm_coefs_i[5][2] = {
        {   0 ,   0 },
        {  60 ,   0 },
        { 115 , -52 },
        {  98 , -55 },
        { 122 , -60 },
#if 0
        /* extended table from PPSSPP (PSP emu), found by tests (unused?) */
        {   0 ,   0 },
        {   0 ,   0 },
        {  52 ,   0 },
        {  55 ,  -2 },
        {  60 ,-125 },
        {   0 ,   0 },
        {   0 , -91 },
        {   0 ,   0 },
        {   2 ,-216 },
        { 125 ,  -6 },
        {   0 ,-151 },
#endif
};


/* Decodes Sony's PS-ADPCM (sometimes called SPU-ADPCM or VAG, just "ADPCM" in the SDK docs).
 * Very similar to XA ADPCM (see xa_decoder for extended info).
 *
 * Some official PC tools decode using float coefs (from the spec), as does this code, but
 * consoles/games/libs would vary (PS1 could do it in hardware using BRR/XA's logic, FMOD/PS3
 * may use int math in software, etc). There are inaudible rounding diffs between implementations.
 */

/* standard PS-ADPCM (float math version) */
void decode_psx(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int is_badflags) {
    off_t frame_offset;
    int i, frames_in, sample_count = 0;
    size_t bytes_per_frame, samples_per_frame;
    uint8_t coef_index, shift_factor, flag;
    int32_t hist1 = stream->adpcm_history1_32;
    int32_t hist2 = stream->adpcm_history2_32;

    /* external interleave (fixed size), mono */
    bytes_per_frame = 0x10;
    samples_per_frame = (bytes_per_frame - 0x02) * 2; /* always 28 */
    frames_in = first_sample / samples_per_frame;
    first_sample = first_sample % samples_per_frame;

    /* parse frame header */
    frame_offset = stream->offset + bytes_per_frame*frames_in;
    coef_index   = ((uint8_t)read_8bit(frame_offset+0x00,stream->streamfile) >> 4) & 0xf;
    shift_factor = ((uint8_t)read_8bit(frame_offset+0x00,stream->streamfile) >> 0) & 0xf;
    flag = (uint8_t)read_8bit(frame_offset+0x01,stream->streamfile); /* only lower nibble needed */

    VGM_ASSERT_ONCE(coef_index > 5 || shift_factor > 12, "PS-ADPCM: incorrect coefs/shift at %x\n", (uint32_t)frame_offset);
    if (coef_index > 5) /* needed by inFamous (PS3) (maybe it's supposed to use more filters?) */
        coef_index = 0; /* upper filters aren't used in PS1/PS2, maybe in PSP/PS3? */
    if (shift_factor > 12)
        shift_factor = 9; /* supposedly, from Nocash PSX docs */

    if (is_badflags) /* some games store garbage or extra internal logic in the flags, must be ignored */
        flag = 0;
    VGM_ASSERT_ONCE(flag > 7,"PS-ADPCM: unknown flag at %x\n", (uint32_t)frame_offset); /* meta should use PSX-badflags */

    /* decode nibbles */
    for (i = first_sample; i < first_sample + samples_to_do; i++) {
        int32_t new_sample = 0;

        if (flag < 0x07) { /* with flag 0x07 decoded sample must be 0 */
            uint8_t nibbles = (uint8_t)read_8bit(frame_offset+0x02+i/2,stream->streamfile);

            new_sample = i&1 ? /* low nibble first */
                    (nibbles >> 4) & 0x0f :
                    (nibbles >> 0) & 0x0f;
            new_sample = (int16_t)((new_sample << 12) & 0xf000) >> shift_factor; /* 16b sign extend + scale */
            new_sample = (int)(new_sample + ps_adpcm_coefs_f[coef_index][0]*hist1 + ps_adpcm_coefs_f[coef_index][1]*hist2);
            new_sample = clamp16(new_sample);
        }

        outbuf[sample_count] = new_sample;
        sample_count += channelspacing;

        hist2 = hist1;
        hist1 = new_sample;
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_history2_32 = hist2;
}


/* PS-ADPCM with configurable frame size and no flag (int math version).
 * Found in some PC/PS3 games (FF XI in sizes 3/5/9/41, Afrika in size 4, Blur/James Bond in size 33, etc).
 *
 * Uses int math to decode, which seems more likely (based on FF XI PC's code in Moogle Toolbox). */
void decode_psx_configurable(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int frame_size) {
    off_t frame_offset;
    int i, frames_in, sample_count = 0;
    size_t bytes_per_frame, samples_per_frame;
    uint8_t coef_index, shift_factor;
    int32_t hist1 = stream->adpcm_history1_32;
    int32_t hist2 = stream->adpcm_history2_32;

    /* external interleave (variable size), mono */
    bytes_per_frame = frame_size;
    samples_per_frame = (bytes_per_frame - 0x01) * 2; /* always 28 */
    frames_in = first_sample / samples_per_frame;
    first_sample = first_sample % samples_per_frame;

    /* parse frame header */
    frame_offset = stream->offset + bytes_per_frame*frames_in;
    coef_index   = ((uint8_t)read_8bit(frame_offset+0x00,stream->streamfile) >> 4) & 0xf;
    shift_factor = ((uint8_t)read_8bit(frame_offset+0x00,stream->streamfile) >> 0) & 0xf;

    VGM_ASSERT_ONCE(coef_index > 5 || shift_factor > 12, "PS-ADPCM: incorrect coefs/shift at %x\n", (uint32_t)frame_offset);
    if (coef_index > 5) /* needed by Afrika (PS3) (maybe it's supposed to use more filters?) */
        coef_index = 0; /* upper filters aren't used in PS1/PS2, maybe in PSP/PS3? */
    if (shift_factor > 12)
        shift_factor = 9; /* supposedly, from Nocash PSX docs */

    /* decode nibbles */
    for (i = first_sample; i < first_sample + samples_to_do; i++) {
        int32_t new_sample = 0;
        uint8_t nibbles = (uint8_t)read_8bit(frame_offset+0x01+i/2,stream->streamfile);

        new_sample = i&1 ? /* low nibble first */
                (nibbles >> 4) & 0x0f :
                (nibbles >> 0) & 0x0f;
        new_sample = (int16_t)((new_sample << 12) & 0xf000) >> shift_factor; /* 16b sign extend + scale */
        new_sample = new_sample + ((ps_adpcm_coefs_i[coef_index][0]*hist1 + ps_adpcm_coefs_i[coef_index][1]*hist2) >> 6);
        new_sample = clamp16(new_sample);

        outbuf[sample_count] = new_sample;
        sample_count += channelspacing;

        hist2 = hist1;
        hist1 = new_sample;
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_history2_32 = hist2;
}


/* Find loop samples in PS-ADPCM data and return if the file loops.
 *
 * PS-ADPCM/VAG has optional bit flags that control looping in the SPU.
 * Possible combinations (as usually defined in Sony's docs):
 * - 0x0 (0000): Normal decode
 * - 0x1 (0001): End marker (last frame)
 * - 0x2 (0010): Loop region (marks files that *may* have loop flags somewhere)
 * - 0x3 (0011): Loop end (jump to loop address)
 * - 0x4 (0100): Start marker
 * - 0x5 (0101): Same as 0x07? Extremely rare [Blood Omen: Legacy of Kain (PS1)]
 * - 0x6 (0110): Loop start (save loop address)
 * - 0x7 (0111): End marker and don't decode
 * - 0x8+(1NNN): Not valid
 */
static int ps_find_loop_offsets_internal(STREAMFILE *streamFile, off_t start_offset, size_t data_size, int channels, size_t interleave, int32_t * out_loop_start, int32_t * out_loop_end, int config) {
    int num_samples = 0, loop_start = 0, loop_end = 0;
    int loop_start_found = 0, loop_end_found = 0;
    off_t offset = start_offset;
    off_t max_offset = start_offset + data_size;
    size_t interleave_consumed = 0;
    int detect_full_loops = config & 1;


    while (offset < max_offset) {
        uint8_t flag = (uint8_t)read_8bit(offset+0x01,streamFile) & 0x0F; /* lower nibble only (for HEVAG) */

        /* theoretically possible and would use last 0x06 */
        VGM_ASSERT_ONCE(loop_start_found && flag == 0x06, "PS LOOPS: multiple loop start found at %x\n", (uint32_t)offset);

        if (flag == 0x06 && !loop_start_found) {
            loop_start = num_samples; /* loop start before this frame */
            loop_start_found = 1;
        }

        if (flag == 0x03 && !loop_end) {
            loop_end = num_samples + 28; /* loop end after this frame */
            loop_end_found = 1;

            /* ignore strange case in Commandos (PS2), has many loop starts and ends */
            if (channels == 1
                    && offset + 0x10 < max_offset
                    && ((uint8_t)read_8bit(offset+0x11,streamFile) & 0x0F) == 0x06) {
                loop_end = 0;
                loop_end_found = 0;
            }

            if (loop_start_found && loop_end_found)
               break;
        }

        /* hack for some games that don't have loop points but do full loops,
         * if there is a "partial" 0x07 end flag pretend it wants to loop
         * (sometimes this will loop non-looping tracks, and won't loop all repeating files) */
        if (flag == 0x01 && detect_full_loops) {
            static const uint8_t eof1[0x10] = {0x00,0x07,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77}; /* common */
            static const uint8_t eof2[0x10] = {0x00,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
          //static const uint8_t eofx[0x10] = {0x07,0x00,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77}; /* sometimes loops */
          //static const uint8_t eofx[0x10] = {0xNN,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}; /* sometimes loops */
            uint8_t buf[0x10];

            int read = read_streamfile(buf,offset+0x10,0x10,streamFile);

            if (read > 0
                    /* also test some extra stuff */
                    && buf[0] != 0x00 /* skip padding */
                    && buf[0] != 0x0c
                    && buf[0] != 0x3c /* skip Ecco the Dolphin (PS2), Ratchet & Clank 2 (PS2), lame hack */
                    ) {

                /* assume full loop if there isn't an EOF tag after current frame */
                if (memcmp(buf,eof1,0x10) != 0 && memcmp(buf,eof2,0x10) != 0) {
                    loop_start = 28; /* skip first frame as it's null in PS-ADPCM */
                    loop_end = num_samples + 28; /* loop end after this frame */
                    loop_start_found = 1;
                    loop_end_found = 1;
                    //;VGM_LOG("PS LOOPS: full loop found\n");
                    break;
                }
            }
        }


        num_samples += 28;
        offset += 0x10;

        /* skip other channels */
        interleave_consumed += 0x10;
        if (interleave_consumed == interleave) {
            interleave_consumed = 0;
            offset += interleave*(channels - 1);
        }
    }

    VGM_ASSERT(loop_start_found && !loop_end_found, "PS LOOPS: found loop start but not loop end\n");
    VGM_ASSERT(loop_end_found && !loop_start_found, "PS LOOPS: found loop end but not loop start\n");
    //;VGM_LOG("PS LOOPS: start=%i, end=%i\n", loop_start, loop_end);

    /* From Sony's docs: if only loop_end is set loop back to "phoneme region start", but in practice doesn't */
    if (loop_start_found && loop_end_found) {
        *out_loop_start = loop_start;
        *out_loop_end = loop_end;
        return 1;
    }

    return 0; /* no loop */
}

int ps_find_loop_offsets(STREAMFILE *streamFile, off_t start_offset, size_t data_size, int channels, size_t interleave, int32_t * out_loop_start, int32_t * out_loop_end) {
    return ps_find_loop_offsets_internal(streamFile, start_offset, data_size, channels, interleave, out_loop_start, out_loop_end, 0);
}

int ps_find_loop_offsets_full(STREAMFILE *streamFile, off_t start_offset, size_t data_size, int channels, size_t interleave, int32_t * out_loop_start, int32_t * out_loop_end) {
    return ps_find_loop_offsets_internal(streamFile, start_offset, data_size, channels, interleave, out_loop_start, out_loop_end, 1);
}

size_t ps_bytes_to_samples(size_t bytes, int channels) {
    return bytes / channels / 0x10 * 28;
}

size_t ps_cfg_bytes_to_samples(size_t bytes, size_t frame_size, int channels) {
    return bytes / channels / frame_size * 28;
}

