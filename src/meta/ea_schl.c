#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"

/* header version */
#define EA_VERSION_NONE         -1
#define EA_VERSION_V0           0x00  // ~early PC (when codec1 was used)
#define EA_VERSION_V1           0x01  // ~PC
#define EA_VERSION_V2           0x02  // ~PS era
#define EA_VERSION_V3           0x03  // ~PS2 era

/* platform constants (unasigned values seem internal only) */
#define EA_PLATFORM_GENERIC     -1    // typically Wii/X360/PS3
#define EA_PLATFORM_PC          0x00
#define EA_PLATFORM_PSX         0x01
#define EA_PLATFORM_N64         0x02
#define EA_PLATFORM_MAC         0x03
#define EA_PLATFORM_SAT         0x04
#define EA_PLATFORM_PS2         0x05
#define EA_PLATFORM_GC_WII      0x06  // reused later for Wii
#define EA_PLATFORM_XBOX        0x07
#define EA_PLATFORM_X360        0x09  // also "Xenon"
#define EA_PLATFORM_PSP         0x0A
#define EA_PLATFORM_3DS         0x14

/* codec constants (undefined are probably reserved, ie.- sx.exe encodes PCM24/DVI but no platform decodes them) */
/* CODEC1 values were used early, then they migrated to CODEC2 values */
#define EA_CODEC1_NONE          -1
#define EA_CODEC1_PCM           0x00
#define EA_CODEC1_VAG           0x01  // unsure
#define EA_CODEC1_EAXA          0x07  // Need for Speed 2 PC, FIFA 98 SAT
#define EA_CODEC1_MT10          0x09
//#define EA_CODEC1_N64         ?

#define EA_CODEC2_NONE          -1
#define EA_CODEC2_MT10          0x04
#define EA_CODEC2_VAG           0x05
#define EA_CODEC2_S16BE         0x07
#define EA_CODEC2_S16LE         0x08
#define EA_CODEC2_S8            0x09
#define EA_CODEC2_EAXA          0x0A
#define EA_CODEC2_LAYER2        0x0F
#define EA_CODEC2_LAYER3        0x10
#define EA_CODEC2_GCADPCM       0x12
#define EA_CODEC2_XBOXADPCM     0x14
#define EA_CODEC2_MT5           0x16
#define EA_CODEC2_EALAYER3      0x17
#define EA_CODEC2_ATRAC3PLUS    0x1B /* Medal of Honor Heroes 2 (PSP) */
//todo #define EA_CODEC2_ATRAC9 0x-- /* supposedly exists */

#define EA_MAX_CHANNELS  6

typedef struct {
    int32_t num_samples;
    int32_t sample_rate;
    int32_t channels;
    int32_t platform;
    int32_t version;
    int32_t bps;
    int32_t codec1;
    int32_t codec2;

    int32_t loop_start;
    int32_t loop_end;

    off_t offsets[EA_MAX_CHANNELS];
    off_t coefs[EA_MAX_CHANNELS];

    int big_endian;
    int loop_flag;
    int codec_version;
} ea_header;

static int parse_variable_header(STREAMFILE* streamFile, ea_header* ea, off_t begin_offset, int max_length);
static uint32_t read_patch(STREAMFILE* streamFile, off_t* offset);
static int get_ea_stream_total_samples(STREAMFILE* streamFile, off_t start_offset, const ea_header* ea);
static off_t get_ea_stream_mpeg_start_offset(STREAMFILE* streamFile, off_t start_offset, const ea_header* ea);
static VGMSTREAM * init_vgmstream_ea_variable_header(STREAMFILE *streamFile, ea_header *ea, off_t start_offset, int is_bnk, int total_streams);


/* EA SCHl with variable header - from EA games (roughly 1997~2010); generated by EA Canada's sx.exe/Sound eXchange */
VGMSTREAM * init_vgmstream_ea_schl(STREAMFILE *streamFile) {
    off_t start_offset, header_offset;
    size_t header_size;
    ea_header ea;


    /* check extension; exts don't seem enforced by EA's tools, but usually:
     * STR/ASF/MUS ~early, EAM ~mid, SNG/AUD ~late, rest uncommon/one game (ex. STRM: MySims Kingdom Wii) */
    if (!check_extensions(streamFile,"str,asf,mus,eam,sng,aud,sx,strm,xa,xsf,exa,stm,ast"))
        goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x5343486C &&  /* "SCHl" */
        read_32bitBE(0x00,streamFile) != 0x5348454E &&  /* "SHEN" */
        read_32bitBE(0x00,streamFile) != 0x53484652)    /* "SHFR" */
        goto fail;

    /* stream is divided into blocks/chunks: SCHl=audio header, SCCl=count of SCDl, SCDl=data xN, SCLl=loop end, SCEl=end.
     * Video uses various blocks (MVhd/MV0K/etc) and sometimes alt audio blocks (SHxx/SCxx/SDxx/SExx where XX=language, EN/FR).
     * The number/size is affected by: block rate setting, sample rate, channels, CPU location (SPU/main/DSP/others), etc */

    header_size = read_32bitLE(0x04,streamFile);
    if (header_size > 0x00F00000) /* size is always LE, except in early SS/MAC */
        header_size = read_32bitBE(0x04,streamFile);
    header_offset = 0x08;

    if (!parse_variable_header(streamFile,&ea, 0x08, header_size - header_offset))
        goto fail;

    start_offset = header_size; /* starts in "SCCl" (skipped in block layout) or very rarely "SCDl" and maybe movie blocks */

    /* rest is common */
    return init_vgmstream_ea_variable_header(streamFile, &ea, start_offset, 0, 1);

fail:
    return NULL;
}

/* EA BNK with variable header - from EA games SFXs; also created by sx.exe */
VGMSTREAM * init_vgmstream_ea_bnk(STREAMFILE *streamFile) {
    off_t start_offset, header_offset, offset, table_offset;
    size_t header_size;
    ea_header ea;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = NULL;
    int i, bnk_version;
    int total_streams, target_stream = streamFile->stream_index;


    /* check extension */
    /* .bnk: sfx, .sdt: speech, .mus: streams/jingles (rare) */
    if (!check_extensions(streamFile,"bnk,sdt,mus"))
        goto fail;

    /* check header (doesn't use EA blocks, otherwise very similar to SCHl) */
    if (read_32bitBE(0x00,streamFile) == 0x424E4B6C ||  /* "BNKl" (common) */
        read_32bitBE(0x00,streamFile) == 0x424E4B62)    /* "BNKb" (FIFA 98 SS) */
        offset = 0;
    else if (read_32bitBE(0x100,streamFile) == 0x424E4B6C)  /* "BNKl" (common) */
        offset = 0x100; /* Harry Potter and the Goblet of Fire (PS2) .mus have weird extra 0x100 bytes */
    else
        goto fail;

    /* use header size as endianness flag */
    if ((uint32_t)read_32bitLE(0x08,streamFile) > 0x000F0000) { /* todo not very accurate */
        read_32bit = read_32bitBE;
        read_16bit = read_16bitBE;
    } else {
        read_32bit = read_32bitLE;
        read_16bit = read_16bitLE;
    }

    bnk_version = read_8bit(offset + 0x04,streamFile);
    total_streams = read_16bit(offset + 0x06,streamFile);
    /* check multi-streams */
    if (target_stream == 0) target_stream = 1;
    if (target_stream < 0 || target_stream > total_streams || total_streams < 1) goto fail;

    switch(bnk_version) {
        case 0x02: /* early (Need For Speed PC, Fifa 98 SS) */
            table_offset = 0x0c;

            header_size = read_32bit(offset + 0x08,streamFile); /* full size */
            header_offset = offset + table_offset + 0x04*(target_stream-1) + read_32bit(offset + table_offset + 0x04*(target_stream-1),streamFile);
            break;

        case 0x04: /* mid (last used in PSX banks) */
        case 0x05: /* late (generated by sx.exe ~v2+) */
            /* 0x08: header/file size, 0x0C: file size/null, 0x10: always null */
            table_offset = 0x14;
            if (read_32bit(offset + table_offset,streamFile) == 0x00)
                table_offset += 0x4; /* MOH Heroes 2 PSP has an extra empty field, not sure why */

            header_size = get_streamfile_size(streamFile); /* unknown (header is variable and may have be garbage until data) */
            header_offset = offset + table_offset + 0x04*(target_stream-1) + read_32bit(offset + table_offset + 0x04*(target_stream-1),streamFile);
            break;

        default:
            VGM_LOG("EA BNK: unknown version %x\n", bnk_version);
            goto fail;
    }

    if (!parse_variable_header(streamFile,&ea, header_offset, header_size - header_offset))
        goto fail;

    /* fix absolute offsets so it works in next funcs */
    if (offset) {
        for (i = 0; i < ea.channels; i++) {
            ea.coefs[i] += offset;
            ea.offsets[i] += offset;
        }
    }

    start_offset = ea.offsets[0]; /* first channel, presumably needed for MPEG */

    /* special case found in some tests (pcstream had hist, pcbnk no hist, no patch diffs)
     * I think this works but what decides if hist is used or not a secret to everybody */
    if (ea.codec2 == EA_CODEC2_EAXA && ea.codec1 == EA_CODEC1_NONE && ea.version == EA_VERSION_V1) {
        ea.codec_version = 0;
    }


    /* rest is common */
    return init_vgmstream_ea_variable_header(streamFile, &ea, start_offset, bnk_version, total_streams);

fail:
    return NULL;
}

/* inits VGMSTREAM from a EA header */
static VGMSTREAM * init_vgmstream_ea_variable_header(STREAMFILE *streamFile, ea_header * ea, off_t start_offset, int bnk_version, int total_streams) {
    VGMSTREAM * vgmstream = NULL;
    int i, ch;
    int is_bnk = bnk_version;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(ea->channels, ea->loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = ea->sample_rate;
    vgmstream->num_samples = ea->num_samples;
    vgmstream->loop_start_sample = ea->loop_start;
    vgmstream->loop_end_sample = ea->loop_end;

    vgmstream->codec_endian = ea->big_endian;
    vgmstream->codec_version = ea->codec_version;

    vgmstream->meta_type = is_bnk ? meta_EA_BNK : meta_EA_SCHL;

    if (is_bnk) {
        vgmstream->layout_type = layout_none;

        /* BNKs usually have absolute offsets for all channels ("full" interleave) except in some versions */
        if (vgmstream->channels > 1 && ea->codec1 == EA_CODEC1_PCM) {
            int interleave = (vgmstream->num_samples * (ea->bps == 8 ? 0x01 : 0x02)); /* full interleave */
            for (i = 0; i < vgmstream->channels; i++) {
                ea->offsets[i] = ea->offsets[0] + interleave*i;
            }
        }
        else if (vgmstream->channels > 1 && ea->codec1 == EA_CODEC1_VAG) {
            int interleave = (vgmstream->num_samples / 28 * 16); /* full interleave */
            for (i = 0; i < vgmstream->channels; i++) {
                ea->offsets[i] = ea->offsets[0] + interleave*i;
            }
        }
        else if (vgmstream->channels > 1 && ea->codec2 == EA_CODEC2_GCADPCM && ea->offsets[0] == ea->offsets[1]) {
            /* pcstream+gcadpcm with sx.exe v2, this is probably an bug (even with this parts of the wave are off) */
            int interleave = (vgmstream->num_samples / 14 * 8); /* full interleave */
            for (i = 0; i < vgmstream->channels; i++) {
                ea->offsets[i] = ea->offsets[0] + interleave*i;
            }
        }
    }
    else {
        vgmstream->layout_type = layout_blocked_ea_schl;
    }

    if (is_bnk)
        vgmstream->num_streams = total_streams;

    /* EA usually implements their codecs in all platforms (PS2/WII do EAXA/MT/EALAYER3) and
     * favors them over platform's natives (ex. EAXA vs VAG/DSP).
     * Unneeded codecs are removed over time (ex. LAYER3 when EALAYER3 was introduced). */
    switch (ea->codec2) {

        case EA_CODEC2_EAXA:        /* EA-XA, CDXA ADPCM variant */
            if (ea->codec1 == EA_CODEC1_EAXA) {
                if (ea->platform != EA_PLATFORM_SAT && ea->channels > 1)
                    vgmstream->coding_type = coding_EA_XA; /* original version, stereo stream */
                else
                    vgmstream->coding_type = coding_EA_XA_int; /* interleaved mono streams */
            }
            else { /* later revision with PCM blocks and slighty modified decoding */
                vgmstream->coding_type = coding_EA_XA_V2;
            }
            break;

        case EA_CODEC2_S8:          /* PCM8 */
            vgmstream->coding_type = coding_PCM8;
            break;

        case EA_CODEC2_S16BE:       /* PCM16BE */
            vgmstream->coding_type = coding_PCM16BE;
            break;

        case EA_CODEC2_S16LE:       /* PCM16LE */
            vgmstream->coding_type = coding_PCM16LE;
            break;

        case EA_CODEC2_VAG:         /* PS-ADPCM */
            vgmstream->coding_type = coding_PSX;
            break;

        case EA_CODEC2_XBOXADPCM:   /* XBOX IMA (interleaved mono) */
            vgmstream->coding_type = coding_XBOX_IMA_int;
            break;

        case EA_CODEC2_GCADPCM:     /* DSP */
            vgmstream->coding_type = coding_NGC_DSP;

            /* get them coefs (start offsets are not necessarily ordered) */
            {
                int16_t (*read_16bit)(off_t,STREAMFILE*) = ea->big_endian ? read_16bitBE : read_16bitLE;

                for (ch=0; ch < vgmstream->channels; ch++) {
                    for (i=0; i < 16; i++) { /* actual size 0x21, last byte unknown */
                        vgmstream->ch[ch].adpcm_coef[i] = read_16bit(ea->coefs[ch] + i*2, streamFile);
                    }
                }
            }
            break;

#ifdef VGM_USE_MPEG
        case EA_CODEC2_LAYER2:      /* MPEG Layer II, aka MP2 */
        case EA_CODEC2_LAYER3: {    /* MPEG Layer III, aka MP3 */
            mpeg_custom_config cfg = {0};
            off_t mpeg_start_offset = is_bnk ?
                    start_offset :
                    get_ea_stream_mpeg_start_offset(streamFile, start_offset, ea);
            if (!mpeg_start_offset) goto fail;

            /* layout is still blocks, but should work fine with the custom mpeg decoder */
            vgmstream->codec_data = init_mpeg_custom(streamFile, mpeg_start_offset, &vgmstream->coding_type, vgmstream->channels, MPEG_EA, &cfg);
            if (!vgmstream->codec_data) goto fail;
            break;
        }

        case EA_CODEC2_EALAYER3: {  /* MP3 variant */
            mpeg_custom_config cfg = {0};
            off_t mpeg_start_offset = is_bnk ?
                    start_offset :
                    get_ea_stream_mpeg_start_offset(streamFile, start_offset, ea);
            if (!mpeg_start_offset) goto fail;

            /* layout is still blocks, but should work fine with the custom mpeg decoder */
            vgmstream->codec_data = init_mpeg_custom(streamFile, mpeg_start_offset, &vgmstream->coding_type, vgmstream->channels, MPEG_EAL31, &cfg);
            if (!vgmstream->codec_data) goto fail;
            break;
        }
#endif

        case EA_CODEC2_MT10:        /* MicroTalk (10:1 compression) */
        case EA_CODEC2_MT5:         /* MicroTalk (5:1 compression) */
            vgmstream->coding_type = coding_EA_MT;
            vgmstream->codec_data = init_ea_mt(vgmstream->channels, ea->version == EA_VERSION_V3);
            if (!vgmstream->codec_data) goto fail;
            break;

        case EA_CODEC2_ATRAC3PLUS:  /* regular ATRAC3plus chunked in SCxx blocks, including RIFF header */
        default:
            VGM_LOG("EA SCHl: unknown codec2 0x%02x for platform 0x%02x\n", ea->codec2, ea->platform);
            goto fail;
    }


    /* open files; channel offsets are updated below */
    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;

    /* fix num_samples for streams with multiple SCHl */
    if (!is_bnk) {
        int total_samples = get_ea_stream_total_samples(streamFile, start_offset, ea);
        if (total_samples > vgmstream->num_samples)
           vgmstream->num_samples = total_samples;
    }


    if (is_bnk) {
        /* setup channel offsets */
        if (vgmstream->coding_type == coding_EA_XA) { /* shared */
            for (i = 0; i < vgmstream->channels; i++) {
                vgmstream->ch[i].offset = ea->offsets[0];
            }
        //} else if (vgmstream->layout_type == layout_interleave) { /* interleaved */
        //    for (i = 0; i < vgmstream->channels; i++) {
        //        vgmstream->ch[i].offset = ea->offsets[0] + vgmstream->interleave_block_size*i;
        //    }
        } else { /* absolute */
            for (i = 0; i < vgmstream->channels; i++) {
                vgmstream->ch[i].offset = ea->offsets[i];
            }
        }

        /* setup ADPCM hist */
        switch(vgmstream->coding_type) {
            /* id, size, samples, hists-per-channel, stereo/interleaved data */
            case coding_EA_XA:
                /* read ADPCM history from all channels before data (not actually read in sx.exe) */
                for (i = 0; i < vgmstream->channels; i++) {
                    //vgmstream->ch[i].adpcm_history1_32 = read_16bit(block_offset + 0x0C + (i*0x04) + 0x00,streamFile);
                    //vgmstream->ch[i].adpcm_history2_32 = read_16bit(block_offset + 0x0C + (i*0x04) + 0x02,streamFile);
                    vgmstream->ch[i].offset += vgmstream->channels*0x04;
                }
                break;

            default:
                /* read ADPCM history before each channel if needed (not actually read in sx.exe) */
                if (vgmstream->codec_version == 1) {
                    for (i = 0; i < vgmstream->channels; i++) {
                        //vgmstream->ch[i].adpcm_history1_32 = read_16bit(vgmstream->ch[i].offset+0x00,streamFile);
                        //vgmstream->ch[i].adpcm_history3_32 = read_16bit(vgmstream->ch[i].offset+0x02,streamFile);
                        vgmstream->ch[i].offset += 4;
                    }
                }
                break;
        }
    }
    else {
        /* setup first block to update offsets */
        block_update_ea_schl(start_offset,vgmstream);
    }


    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


static uint32_t read_patch(STREAMFILE* streamFile, off_t* offset) {
    uint32_t result = 0;
    uint8_t byte_count = read_8bit(*offset, streamFile);
    (*offset)++;

    if (byte_count == 0xFF) { /* signals 32b size (ex. custom user data) */
        (*offset) += 4 + read_32bitBE(*offset, streamFile);
        return 0;
    }

    if (byte_count > 4) { /* uncommon (ex. coef patches) */
        (*offset) += byte_count;
        return 0;
    }

    for ( ; byte_count > 0; byte_count--) { /* count of 0 is also possible, means value 0 */
        result <<= 8;
        result += (uint8_t)read_8bit(*offset, streamFile);
        (*offset)++;
    }

    return result;
}

/* decodes EA's GSTR/PT header (mostly cross-referenced with sx.exe) */
static int parse_variable_header(STREAMFILE* streamFile, ea_header* ea, off_t begin_offset, int max_length) {
    off_t offset = begin_offset;
    uint32_t platform_id;
    int is_header_end = 0;

    memset(ea,0,sizeof(ea_header));

    /* null defaults as 0 can be valid */
    ea->version = EA_VERSION_NONE;
    ea->codec1 = EA_CODEC1_NONE;
    ea->codec2 = EA_CODEC2_NONE;

    /* get platform info */
    platform_id = read_32bitBE(offset, streamFile);
    if (platform_id != 0x47535452 && (platform_id & 0xFFFF0000) != 0x50540000) {
        offset += 4; /* skip unknown field (related to blocks/size?) in "nbapsstream" (NBA2000 PS, FIFA2001 PS) */
        platform_id = read_32bitBE(offset, streamFile);
    }
    if (platform_id == 0x47535452) { /* "GSTR" = Generic STReam */
        ea->platform = EA_PLATFORM_GENERIC;
        offset += 4 + 4; /* GSTRs have an extra field (config?): ex. 0x01000000, 0x010000D8 BE */
    }
    else if ((platform_id & 0xFFFF0000) == 0x50540000) { /* "PT" = PlaTform */
        ea->platform = (uint8_t)read_16bitLE(offset + 2,streamFile);
        offset += 4;
    }
    else {
        goto fail;
    }


    /* parse mini-chunks/tags (variable, ommited if default exists; some are removed in later versions of sx.exe) */
    while (!is_header_end && offset - begin_offset < max_length) {
        uint8_t patch_type = read_8bit(offset,streamFile);
        offset++;

        switch(patch_type) {
            case 0x00: /* signals non-default block rate and maybe other stuff; or padding after 0xFF */
                if (!is_header_end)
                    read_patch(streamFile, &offset);
                break;

            case 0x05: /* unknown (usually 0x50 except Madden NFL 3DS: 0x3e800) */
            case 0x06: /* priority (0..100, always 0x65 for streams, others for BNKs; rarely ommited) */
            case 0x07: /* unknown (BNK only: 36|3A) */
            case 0x08: /* release envelope (BNK only) */
            case 0x09: /* related to playback envelope (BNK only) */
            case 0x0A: /* bend range (BNK only) */
            case 0x0B: /* unknown (always 0x02) */
            case 0x0C: /* pan offset (BNK only) */
            case 0x0D: /* random pan offset range (BNK only) */
            case 0x0E: /* volume (BNK only) */
            case 0x0F: /* random volume range (BNK only) */
            case 0x10: /* detune (BNK only) */
            case 0x11: /* random detune range (BNK only) */
            case 0x13: /* effect bus (0..127) */
            case 0x14: /* emdedded user data (free size/value) */
            case 0x19: /* related to playback envelope (BNK only) */
            case 0x1B: /* unknown (movie only?) */
            case 0x1C: /* initial envelope volume (BNK only) */
            case 0x24: /* master random detune range (BNK only) */
                read_patch(streamFile, &offset);
                break;

            case 0xFC: /* padding for alignment between patches */
            case 0xFE: /* padding? (actually exists?) */
            case 0xFD: /* info section start marker */
                break;

            case 0x83: /* codec1 defines, used early revisions */
                ea->codec1 = read_patch(streamFile, &offset);
                break;
            case 0xA0: /* codec2 defines */
                ea->codec2 = read_patch(streamFile, &offset);
                break;

            case 0x80: /* version, affecting some codecs */
                ea->version = read_patch(streamFile, &offset);
                break;
            case 0x81: /* bits per sample for codec1 PCM */
                ea->bps = read_patch(streamFile, &offset);
                break;

            case 0x82: /* channel count */
                ea->channels = read_patch(streamFile, &offset);
                break;
            case 0x84: /* sample rate */
                ea->sample_rate = read_patch(streamFile,&offset);
                break;

            case 0x85: /* sample count */
                ea->num_samples = read_patch(streamFile, &offset);
                break;
            case 0x86: /* loop start sample */
                ea->loop_start = read_patch(streamFile, &offset);
                break;
            case 0x87: /* loop end sample */
                ea->loop_end = read_patch(streamFile, &offset);
                break;

            /* channel offsets (BNK only), can be the equal for all channels or interleaved; not necessarily contiguous */
            case 0x88: /* absolute offset of ch1 (or ch1+ch2 for stereo EAXA) */
                ea->offsets[0] = read_patch(streamFile, &offset);
                break;
            case 0x89: /* absolute offset of ch2 */
                ea->offsets[1] = read_patch(streamFile, &offset);
                break;
            case 0x94: /* absolute offset of ch3 */
                ea->offsets[2] = read_patch(streamFile, &offset);
                break;
            case 0x95: /* absolute offset of ch4 */
                ea->offsets[3] = read_patch(streamFile, &offset);
                break;
            case 0xA2: /* absolute offset of ch5 */
                ea->offsets[4] = read_patch(streamFile, &offset);
                break;
            case 0xA3: /* absolute offset of ch6 */
                ea->offsets[5] = read_patch(streamFile, &offset);
                break;

            case 0x8F: /* DSP/N64BLK coefs ch1 */
                ea->coefs[0] = offset+1;
                read_patch(streamFile, &offset);
                break;
            case 0x90: /* DSP/N64BLK coefs ch2 */
                ea->coefs[1] = offset+1;
                read_patch(streamFile, &offset);
                break;
            case 0x91: /* DSP coefs ch3, and unknown in older versions */
                ea->coefs[2] = offset+1;
                read_patch(streamFile, &offset);
                break;
            case 0xAB: /* DSP coefs ch4 */
                ea->coefs[3] = offset+1;
                read_patch(streamFile, &offset);
                break;
            case 0xAC: /* DSP coefs ch5 */
                ea->coefs[4] = offset+1;
                read_patch(streamFile, &offset);
                break;
            case 0xAD: /* DSP coefs ch6 */
                ea->coefs[5] = offset+1;
                read_patch(streamFile, &offset);
                break;

            case 0x8A: /* long padding (always 0x00000000) */
            case 0x8C: /* flags (ex. play type = 01=static/02=dynamic | spatialize = 20=pan/etc) */
                       /* (ex. PS1 VAG=0, PS2 PCM/LAYER2=4, GC EAXA=4, 3DS DSP=512, Xbox EAXA=36, N64 BLK=05E800, N64 MT10=01588805E800) */
            case 0x92: /* bytes per sample? */
            case 0x98: /* embedded time stretch 1 (long data for who-knows-what) */
            case 0x99: /* embedded time stretch 2 */
            case 0x9C: /* azimuth ch1 */
            case 0x9D: /* azimuth ch2 */
            case 0x9E: /* azimuth ch3 */
            case 0x9F: /* azimuth ch4 */
            case 0xA6: /* azimuth ch5 */
            case 0xA7: /* azimuth ch6 */
            case 0xA1: /* unknown and very rare, always 0x02 (FIFA 2001 PS2) */
                read_patch(streamFile, &offset);
                break;

            case 0xFF: /* header end (then 0-padded so it's 32b aligned) */
                is_header_end = 1;
                break;

            default:
                VGM_LOG("EA SCHl: unknown patch 0x%02x at 0x%04lx\n", patch_type, (offset-1));
                break;
        }
    }

    if (ea->channels > EA_MAX_CHANNELS)
        goto fail;


    /* Set defaults per platform, as the header ommits them when possible */

    ea->loop_flag = (ea->loop_end);

    /* affects blocks/codecs */
    if (ea->platform == EA_PLATFORM_N64
        || ea->platform == EA_PLATFORM_MAC
        || ea->platform == EA_PLATFORM_SAT
        || ea->platform == EA_PLATFORM_GC_WII
        || ea->platform == EA_PLATFORM_X360
        || ea->platform == EA_PLATFORM_GENERIC) {
        ea->big_endian = 1;
    }

    if (!ea->channels) {
        ea->channels = 1;
    }

    /* version mainly affects defaults and minor stuff, can come with all codecs */
    /* V0 is often just null but it's specified in some files (uncommon, with patch size 0x00) */
    if (ea->version == EA_VERSION_NONE) {
        switch(ea->platform) {
            case EA_PLATFORM_PC:        ea->version = EA_VERSION_V0; break;
            case EA_PLATFORM_PSX:       ea->version = EA_VERSION_V0; break; // assumed
            case EA_PLATFORM_N64:       ea->version = EA_VERSION_V0; break;
            case EA_PLATFORM_MAC:       ea->version = EA_VERSION_V0; break;
            case EA_PLATFORM_SAT:       ea->version = EA_VERSION_V0; break;
            case EA_PLATFORM_PS2:       ea->version = EA_VERSION_V1; break;
            case EA_PLATFORM_GC_WII:    ea->version = EA_VERSION_V2; break;
            case EA_PLATFORM_XBOX:      ea->version = EA_VERSION_V2; break;
            case EA_PLATFORM_X360:      ea->version = EA_VERSION_V3; break;
            case EA_PLATFORM_PSP:       ea->version = EA_VERSION_V3; break;
            case EA_PLATFORM_3DS:       ea->version = EA_VERSION_V3; break;
            case EA_PLATFORM_GENERIC:   ea->version = EA_VERSION_V2; break;
            default:
                VGM_LOG("EA SCHl: unknown default version for platform 0x%02x\n", ea->platform);
                goto fail;
        }
    }

    /* codec1 defaults */
    if (ea->codec1 == EA_CODEC1_NONE && ea->version == EA_VERSION_V0) {
        switch(ea->platform) {
            case EA_PLATFORM_PC:        ea->codec1 = EA_CODEC1_PCM; break;
            case EA_PLATFORM_PSX:       ea->codec1 = EA_CODEC1_VAG; break; // assumed
            //case EA_PLATFORM_N64:     ea->codec1 = EA_CODEC1_N64; break;
            case EA_PLATFORM_MAC:       ea->codec1 = EA_CODEC1_PCM; break; // assumed
            case EA_PLATFORM_SAT:       ea->codec1 = EA_CODEC1_PCM; break;
            default:
                VGM_LOG("EA SCHl: unknown default codec1 for platform 0x%02x\n", ea->platform);
                goto fail;
        }
    }

    /* codec1 to codec2 to simplify later parsing */
    if (ea->codec1 != EA_CODEC1_NONE && ea->codec2 == EA_CODEC2_NONE) {
        switch (ea->codec1) {
            case EA_CODEC1_PCM:
                ea->codec2 = ea->bps==8 ? EA_CODEC2_S8 : (ea->big_endian ? EA_CODEC2_S16BE : EA_CODEC2_S16LE);
                break;
            case EA_CODEC1_VAG:         ea->codec2 = EA_CODEC2_VAG; break;
            case EA_CODEC1_EAXA:        ea->codec2 = EA_CODEC2_EAXA; break;
            case EA_CODEC1_MT10:        ea->codec2 = EA_CODEC2_MT10; break;
            default:
                VGM_LOG("EA SCHl: unknown codec1 0x%02x\n", ea->codec1);
                goto fail;
        }
    }

    /* codec2 defaults */
    if (ea->codec2 == EA_CODEC2_NONE) {
        switch(ea->platform) {
            case EA_PLATFORM_GENERIC:   ea->codec2 = EA_CODEC2_EAXA; break;
            case EA_PLATFORM_PC:        ea->codec2 = EA_CODEC2_EAXA; break;
            case EA_PLATFORM_PSX:       ea->codec2 = EA_CODEC2_VAG; break;
            case EA_PLATFORM_MAC:       ea->codec2 = EA_CODEC2_EAXA; break;
            case EA_PLATFORM_PS2:       ea->codec2 = EA_CODEC2_VAG; break;
            case EA_PLATFORM_GC_WII:    ea->codec2 = EA_CODEC2_S16BE; break;
            case EA_PLATFORM_XBOX:      ea->codec2 = EA_CODEC2_S16LE; break;
            case EA_PLATFORM_X360:      ea->codec2 = EA_CODEC2_EAXA; break;
            case EA_PLATFORM_PSP:       ea->codec2 = EA_CODEC2_EAXA; break;
            case EA_PLATFORM_3DS:       ea->codec2 = EA_CODEC2_GCADPCM; break;
            default:
                VGM_LOG("EA SCHl: unknown default codec2 for platform 0x%02x\n", ea->platform);
                goto fail;
        }
    }

    /* somehow doesn't follow machine's sample rate or anything sensical */
    if (!ea->sample_rate) {
        switch(ea->platform) {
            case EA_PLATFORM_GENERIC:   ea->sample_rate = 48000; break;
            case EA_PLATFORM_PC:        ea->sample_rate = 22050; break;
            case EA_PLATFORM_PSX:       ea->sample_rate = 22050; break;
            case EA_PLATFORM_N64:       ea->sample_rate = 22050; break;
            case EA_PLATFORM_MAC:       ea->sample_rate = 22050; break;
            case EA_PLATFORM_SAT:       ea->sample_rate = 22050; break;
            case EA_PLATFORM_PS2:       ea->sample_rate = 22050; break;
            case EA_PLATFORM_GC_WII:    ea->sample_rate = 24000; break;
            case EA_PLATFORM_XBOX:      ea->sample_rate = 24000; break;
            case EA_PLATFORM_X360:      ea->sample_rate = 44100; break;
            case EA_PLATFORM_PSP:       ea->sample_rate = 22050; break;
            //case EA_PLATFORM_3DS:     ea->sample_rate = 44100; break;//todo (not 22050/16000)
            default:
                VGM_LOG("EA SCHl: unknown default sample rate for platform 0x%02x\n", ea->platform);
                goto fail;
        }
    }

    /* special flag: 1=has ADPCM history per block, 0=doesn't */
    if (ea->codec2 == EA_CODEC2_GCADPCM && ea->platform == EA_PLATFORM_3DS) {
        ea->codec_version = 1;
    }
    else if (ea->codec2 == EA_CODEC2_EAXA && ea->codec1 == EA_CODEC1_NONE) {
        /* console V2 uses hist, as does PC/MAC V1 (but not later versions) */
        if (ea->version <= EA_VERSION_V1 ||
                ((ea->platform == EA_PLATFORM_PS2 || ea->platform == EA_PLATFORM_GC_WII || ea->platform == EA_PLATFORM_XBOX)
                && ea->version == EA_VERSION_V2)) {
            ea->codec_version = 1;
        }
    }


    return offset;

fail:
    return 0;
}

/* Get total samples by parsing block headers, needed when multiple files are stitched together.
 * Some EA files (.mus/eam/sng/etc) concat many small subfiles, used for interactive/mapped
 * music (.map/lin). Subfiles always share header, except num_samples. */
static int get_ea_stream_total_samples(STREAMFILE* streamFile, off_t start_offset, const ea_header* ea) {
    int num_samples = 0;
    int new_schl = 0;
    off_t block_offset = start_offset;
    size_t block_size, block_samples;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = ea->big_endian ? read_32bitBE : read_32bitLE;
    size_t file_size = get_streamfile_size(streamFile);


    while (block_offset < file_size) {
        uint32_t id = read_32bitBE(block_offset+0x00,streamFile);

        block_size = read_32bitLE(block_offset+0x04,streamFile);
        if (block_size > 0x00F00000) /* size is always LE, except in early SAT/MAC */
            block_size = read_32bitBE(block_offset+0x04,streamFile);

        block_samples = 0;

        if (id == 0x5343446C || id == 0x5344454E || id == 0x53444652) { /* "SCDl" "SDEN" "SDFR" audio data */
            switch (ea->codec2) {
                case EA_CODEC2_VAG:
                    block_samples = ps_bytes_to_samples(block_size-0x10, ea->channels);
                    break;
                default:
                    block_samples = read_32bit(block_offset+0x08,streamFile);
                    break;
            }
        }
        else { /* any other chunk, audio ("SCHl" "SCCl" "SCLl" "SCEl" etc), or video ("pQGT" "pIQT "MADk" "MPCh" etc) */
            /* padding between "SCEl" and next "SCHl" (when subfiles exist) */
            if (id == 0x00000000) {
                block_size = 0x04;
            }

            if (id == 0x5343486C || id == 0x5348454E || id == 0x53484652) { /* "SCHl" "SHEN" "SHFR" end block */
                new_schl = 1;
            }
        }

        /* guard against errors (happens in bad rips/endianness, observed max is vid ~0x20000) */
        if (block_size == 0x00 || block_size > 0xFFFFF || block_samples > 0xFFFF) {
            VGM_LOG("EA SCHl: bad block size %x at %lx\n", block_size, block_offset);
            block_size = 0x04;
            block_samples = 0;
        }

        num_samples += block_samples;
        block_offset += block_size;

        /* "SCEl" "SEEN" "SEFR" are aligned to 0x80 usually, but causes problems if not 32b-aligned (ex. Need for Speed 2 PC) */
        if ((id == 0x5343456C || id == 0x5345454E || id == 0x53454652) && block_offset % 0x04) {
            VGM_LOG_ONCE("EA SCHl: mis-aligned end offset found\n");
            block_offset += 0x04 - (block_offset % 0x04);
        }
    }

    /* only use calculated samples with multiple subfiles (rarely header samples may be less due to padding) */
    if (new_schl) {
        ;VGM_LOG("EA SCHl: multiple SCHl found\n");
        return num_samples;
    }
    else {
        return 0;
    }
}

/* find data start offset inside the first SCDl; not very elegant but oh well */
static off_t get_ea_stream_mpeg_start_offset(STREAMFILE* streamFile, off_t start_offset, const ea_header* ea) {
    size_t file_size = get_streamfile_size(streamFile);
    off_t block_offset = start_offset;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = ea->big_endian ? read_32bitBE : read_32bitLE;

    while (block_offset < file_size) {
        uint32_t id, block_size;

        id = read_32bitBE(block_offset+0x00,streamFile);

        block_size = read_32bitLE(block_offset+0x04,streamFile);
        if (block_size > 0x00F00000) /* size is always LE, except in early SAT/MAC */
            block_size = read_32bitBE(block_offset+0x04,streamFile);

        if (id == 0x5343446C || id == 0x5344454E || id == 0x53444652) { /* "SCDl/SDEN/SDFR" data block found */
            off_t offset = read_32bit(block_offset+0x0c,streamFile); /* first value seems ok, second is something else in EALayer3 */
            return block_offset + 0x0c + ea->channels*0x04 + offset;
        } else if (id == 0x5343436C || id == 0x5343454E || id == 0x53434652) { /* "SCCl/SCEN/SCFR" data count found */
            block_offset += block_size; /* size includes header */
            continue;
        } else {
            goto fail;
        }
    }

fail:
    return 0;
}
