/*
 * vgmstream.h - definitions for VGMSTREAM, encapsulating a multi-channel, looped audio stream
 */

#ifndef _VGMSTREAM_H
#define _VGMSTREAM_H

enum { PATH_LIMIT = 32768 };
enum { STREAM_NAME_SIZE = 255 }; /* reasonable max */

#include "streamfile.h"

/* Due mostly to licensing issues, Vorbis, MPEG, G.722.1, etc decoding is done by external libraries.
 * Libs are disabled by default, defined on compile-time for builds that support it */
//#define VGM_USE_VORBIS
//#define VGM_USE_MPEG
//#define VGM_USE_G7221
//#define VGM_USE_G719
//#define VGM_USE_MP4V2
//#define VGM_USE_FDKAAC
//#define VGM_USE_MAIATRAC3PLUS
//#define VGM_USE_FFMPEG
//#define VGM_USE_ATRAC9
//#define VGM_USE_CELT


#ifdef VGM_USE_VORBIS
#include <vorbis/vorbisfile.h>
#endif

#ifdef VGM_USE_MPEG
#include <mpg123.h>
#endif

#ifdef VGM_USE_G7221
#include <g7221.h>
#endif

#ifdef VGM_USE_MP4V2
#define MP4V2_NO_STDINT_DEFS
#include <mp4v2/mp4v2.h>
#endif

#ifdef VGM_USE_FDKAAC
#include <aacdecoder_lib.h>
#endif

#ifdef VGM_USE_MAIATRAC3PLUS
#include <maiatrac3plus.h>
#endif

#ifdef VGM_USE_FFMPEG
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#endif

#include <clHCA.h>

#include "coding/g72x_state.h"
#include "coding/nwa_decoder.h"


/* The encoding type specifies the format the sound data itself takes */
typedef enum {
    /* PCM */
    coding_PCM16LE,         /* little endian 16-bit PCM */
    coding_PCM16BE,         /* big endian 16-bit PCM */
    coding_PCM16_int,       /* 16-bit PCM with sample-level interleave (for blocks) */

    coding_PCM8,            /* 8-bit PCM */
    coding_PCM8_int,        /* 8-bit PCM with sample-level interleave (for blocks) */
    coding_PCM8_U,          /* 8-bit PCM, unsigned (0x80 = 0) */
    coding_PCM8_U_int,      /* 8-bit PCM, unsigned (0x80 = 0) with sample-level interleave (for blocks) */
    coding_PCM8_SB,         /* 8-bit PCM, sign bit (others are 2's complement) */
    coding_PCM4,            /* 4-bit PCM, signed */
    coding_PCM4_U,          /* 4-bit PCM, unsigned */

    coding_ULAW,            /* 8-bit u-Law (non-linear PCM) */
    coding_ULAW_int,        /* 8-bit u-Law (non-linear PCM) with sample-level interleave (for blocks) */
    coding_ALAW,            /* 8-bit a-Law (non-linear PCM) */

    coding_PCMFLOAT,        /* 32 bit float PCM */

    /* ADPCM */
    coding_CRI_ADX,         /* CRI ADX */
    coding_CRI_ADX_fixed,   /* CRI ADX, encoding type 2 with fixed coefficients */
    coding_CRI_ADX_exp,     /* CRI ADX, encoding type 4 with exponential scale */
    coding_CRI_ADX_enc_8,   /* CRI ADX, type 8 encryption (God Hand) */
    coding_CRI_ADX_enc_9,   /* CRI ADX, type 9 encryption (PSO2) */

    coding_NGC_DSP,         /* Nintendo DSP ADPCM */
    coding_NGC_DSP_subint,  /* Nintendo DSP ADPCM with frame subinterframe */
    coding_NGC_DTK,         /* Nintendo DTK ADPCM (hardware disc), also called TRK or ADP */
    coding_NGC_AFC,         /* Nintendo AFC ADPCM */

    coding_G721,            /* CCITT G.721 */

    coding_XA,              /* CD-ROM XA */
    coding_PSX,             /* Sony PS ADPCM (VAG) */
    coding_PSX_badflags,    /* Sony PS ADPCM with custom flag byte */
    coding_PSX_cfg,         /* Sony PS ADPCM with configurable frame size (FF XI, SGXD type 5, Bizarre Creations) */
    coding_HEVAG,           /* Sony PSVita ADPCM */

    coding_EA_XA,           /* Electronic Arts EA-XA ADPCM v1 (stereo) aka "EA ADPCM" */
    coding_EA_XA_int,       /* Electronic Arts EA-XA ADPCM v1 (mono/interleave) */
    coding_EA_XA_V2,        /* Electronic Arts EA-XA ADPCM v2 */
    coding_MAXIS_XA,        /* Maxis EA-XA ADPCM */
    coding_EA_XAS_V0,       /* Electronic Arts EA-XAS ADPCM v0 */
    coding_EA_XAS_V1,       /* Electronic Arts EA-XAS ADPCM v1 */

    coding_IMA,             /* IMA ADPCM (stereo or mono, low nibble first) */
    coding_IMA_int,         /* IMA ADPCM (mono/interleave, low nibble first) */
    coding_DVI_IMA,         /* DVI IMA ADPCM (stereo or mono, high nibble first) */
    coding_DVI_IMA_int,     /* DVI IMA ADPCM (mono/interleave, high nibble first) */
    coding_3DS_IMA,         /* 3DS IMA ADPCM */
    coding_SNDS_IMA,        /* Heavy Iron Studios .snds IMA ADPCM */
    coding_OTNS_IMA,        /* Omikron The Nomad Soul IMA ADPCM */
    coding_WV6_IMA,         /* Gorilla Systems WV6 4-bit IMA ADPCM */
    coding_ALP_IMA,         /* High Voltage ALP 4-bit IMA ADPCM */
    coding_FFTA2_IMA,       /* Final Fantasy Tactics A2 4-bit IMA ADPCM */

    coding_MS_IMA,          /* Microsoft IMA ADPCM */
    coding_XBOX_IMA,        /* XBOX IMA ADPCM */
    coding_XBOX_IMA_mch,    /* XBOX IMA ADPCM (multichannel) */
    coding_XBOX_IMA_int,    /* XBOX IMA ADPCM (mono/interleave) */
    coding_NDS_IMA,         /* IMA ADPCM w/ NDS layout */
    coding_DAT4_IMA,        /* Eurocom 'DAT4' IMA ADPCM */
    coding_RAD_IMA,         /* Radical IMA ADPCM */
    coding_RAD_IMA_mono,    /* Radical IMA ADPCM (mono/interleave) */
    coding_APPLE_IMA4,      /* Apple Quicktime IMA4 */
    coding_FSB_IMA,         /* FMOD's FSB multichannel IMA ADPCM */
    coding_WWISE_IMA,       /* Audiokinetic Wwise IMA ADPCM */
    coding_REF_IMA,         /* Reflections IMA ADPCM */
    coding_AWC_IMA,         /* Rockstar AWC IMA ADPCM */
    coding_UBI_IMA,         /* Ubisoft IMA ADPCM */
    coding_H4M_IMA,         /* H4M IMA ADPCM (stereo or mono, high nibble first) */

    coding_MSADPCM,         /* Microsoft ADPCM (stereo/mono) */
    coding_MSADPCM_int,     /* Microsoft ADPCM (mono) */
    coding_MSADPCM_ck,      /* Microsoft ADPCM (Cricket Audio variation) */
    coding_WS,              /* Westwood Studios VBR ADPCM */
    coding_AICA,            /* Yamaha AICA ADPCM (stereo) */
    coding_AICA_int,        /* Yamaha AICA ADPCM (mono/interleave) */
    coding_YAMAHA,          /* Yamaha ADPCM */
    coding_YAMAHA_NXAP,     /* Yamaha ADPCM (NXAP variation) */
    coding_NDS_PROCYON,     /* Procyon Studio ADPCM */
    coding_L5_555,          /* Level-5 0x555 ADPCM */
    coding_LSF,             /* lsf ADPCM (Fastlane Street Racing iPhone)*/
    coding_MTAF,            /* Konami MTAF ADPCM */
    coding_MTA2,            /* Konami MTA2 ADPCM */
    coding_MC3,             /* Paradigm MC3 3-bit ADPCM */
    coding_FADPCM,          /* FMOD FADPCM 4-bit ADPCM */
    coding_ASF,             /* Argonaut ASF 4-bit ADPCM */
    coding_XMD,             /* Konami XMD 4-bit ADPCM */
    coding_PCFX,            /* PC-FX 4-bit ADPCM */
    coding_OKI16,           /* OKI 4-bit ADPCM with 16-bit output */

    /* others */
    coding_SDX2,            /* SDX2 2:1 Squareroot-Delta-Exact compression DPCM */
    coding_SDX2_int,        /* SDX2 2:1 Squareroot-Delta-Exact compression with sample-level interleave */
    coding_CBD2,            /* CBD2 2:1 Cuberoot-Delta-Exact compression DPCM */
    coding_CBD2_int,        /* CBD2 2:1 Cuberoot-Delta-Exact compression, with sample-level interleave  */
    coding_SASSC,           /* Activision EXAKT SASSC 8-bit DPCM */
    coding_DERF,            /* DERF 8-bit DPCM */
    coding_ACM,             /* InterPlay ACM */
    coding_NWA,             /* VisualArt's NWA */
    coding_CIRCUS_ADPCM,    /* Circus 8-bit ADPCM */

    coding_EA_MT,           /* Electronic Arts MicroTalk (linear-predictive speech codec) */

    coding_CRI_HCA,         /* CRI High Compression Audio (MDCT-based) */

#ifdef VGM_USE_VORBIS
    coding_OGG_VORBIS,      /* Xiph Vorbis with Ogg layer (MDCT-based) */
    coding_VORBIS_custom,   /* Xiph Vorbis with custom layer (MDCT-based) */
#endif

#ifdef VGM_USE_MPEG
    coding_MPEG_custom,     /* MPEG audio with custom features (MDCT-based) */
    coding_MPEG_ealayer3,   /* EALayer3, custom MPEG frames */
    coding_MPEG_layer1,     /* MP1 MPEG audio (MDCT-based) */
    coding_MPEG_layer2,     /* MP2 MPEG audio (MDCT-based) */
    coding_MPEG_layer3,     /* MP3 MPEG audio (MDCT-based) */
#endif

#ifdef VGM_USE_G7221
    coding_G7221C,          /* ITU G.722.1 annex C (Polycom Siren 14) */
#endif

#ifdef VGM_USE_G719
    coding_G719,            /* ITU G.719 annex B (Polycom Siren 22) */
#endif

#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
    coding_MP4_AAC,         /* AAC (MDCT-based) */
#endif

#ifdef VGM_USE_MAIATRAC3PLUS
    coding_AT3plus,         /* Sony ATRAC3plus (MDCT-based) */
#endif

#ifdef VGM_USE_ATRAC9
    coding_ATRAC9,          /* Sony ATRAC9 (MDCT-based) */
#endif

#ifdef VGM_USE_CELT
    coding_CELT_FSB,        /* Custom Xiph CELT (MDCT-based) */
#endif

#ifdef VGM_USE_FFMPEG
    coding_FFmpeg,          /* Formats handled by FFmpeg (ATRAC3, XMA, AC3, etc) */
#endif
} coding_t;

/* The layout type specifies how the sound data is laid out in the file */
typedef enum {
    /* generic */
    layout_none,            /* straight data */

    /* interleave */
    layout_interleave,      /* equal interleave throughout the stream */

    /* headered blocks */
    layout_blocked_ast,
    layout_blocked_halpst,
    layout_blocked_xa,
    layout_blocked_ea_schl,
    layout_blocked_ea_1snh,
    layout_blocked_caf,
    layout_blocked_wsi,
    layout_blocked_str_snds,
    layout_blocked_ws_aud,
    layout_blocked_matx,
    layout_blocked_dec,
    layout_blocked_xvas,
    layout_blocked_vs,
    layout_blocked_mul,
    layout_blocked_gsb,
    layout_blocked_thp,
    layout_blocked_filp,
    layout_blocked_ea_swvr,
    layout_blocked_adm,
    layout_blocked_bdsp,
    layout_blocked_mxch,
    layout_blocked_ivaud,   /* GTA IV .ivaud blocks */
    layout_blocked_tra,     /* DefJam Rapstar .tra blocks */
    layout_blocked_ps2_iab,
    layout_blocked_vs_str,
    layout_blocked_rws,
    layout_blocked_hwas,
    layout_blocked_ea_sns,  /* newest Electronic Arts blocks, found in SNS/SNU/SPS/etc formats */
    layout_blocked_awc,     /* Rockstar AWC */
    layout_blocked_vgs,     /* Guitar Hero II (PS2) */
    layout_blocked_vawx,    /* No More Heroes 6ch (PS3) */
    layout_blocked_xvag_subsong, /* XVAG subsongs [God of War III (PS4)] */
    layout_blocked_ea_wve_au00, /* EA WVE au00 blocks */
    layout_blocked_ea_wve_ad10, /* EA WVE Ad10 blocks */
    layout_blocked_sthd, /* Dream Factory STHD */
    layout_blocked_h4m, /* H4M video */
    layout_blocked_xa_aiff, /* XA in AIFF files [Crusader: No Remorse (SAT), Road Rash (3DO)] */
    layout_blocked_vs_square,

    /* otherwise odd */
    layout_aix,             /* CRI AIX's wheels within wheels */
    layout_segmented,       /* song divided in segments (song sections) */
    layout_layered,         /* song divided in layers (song channels) */

} layout_t;

/* The meta type specifies how we know what we know about the file.
 * We may know because of a header we read, some of it may have been guessed from filenames, etc. */
typedef enum {

    meta_DSP_STD,           /* Nintendo standard GC ADPCM (DSP) header */
    meta_DSP_CSTR,          /* Star Fox Assault "Cstr" */
    meta_DSP_RS03,          /* Retro: Metroid Prime 2 "RS03" */
    meta_DSP_STM,           /* Paper Mario 2 STM */
    meta_AGSC,              /* Retro: Metroid Prime 2 title */
    meta_CSMP,              /* Retro: Metroid Prime 3 (Wii), Donkey Kong Country Returns (Wii) */
    meta_RFRM,              /* Retro: Donkey Kong Country Tropical Freeze (Wii U) */
    meta_DSP_MPDSP,         /* Monopoly Party single header stereo */
    meta_DSP_JETTERS,       /* Bomberman Jetters .dsp */
    meta_DSP_MSS,           /* Free Radical GC games */
    meta_DSP_GCM,           /* some of Traveller's Tales games */
    meta_DSP_STR,           /* Conan .str files */
    meta_DSP_SADB,          /* .sad */
    meta_DSP_WSI,           /* .wsi */
    meta_IDSP_TT,           /* Traveller's Tales games */
    meta_DSP_WII_MUS,       /* .mus */
    meta_DSP_WII_WSD,       /* Phantom Brave (WII) */
    meta_WII_NDP,           /* Vertigo (Wii) */
    meta_DSP_YGO,           /* Konami: Yu-Gi-Oh! The Falsebound Kingdom (NGC), Hikaru no Go 3 (NGC) */
    meta_DSP_SADF,          /* Procyon Studio SADF - Xenoblade Chronicles 2 (Switch) */

    meta_STRM,              /* Nintendo STRM */
    meta_RSTM,              /* Nintendo RSTM (Revolution Stream, similar to STRM) */
    meta_AFC,               /* AFC */
    meta_AST,               /* AST */
    meta_RWSD,              /* single-stream RWSD */
    meta_RWAR,              /* single-stream RWAR */
    meta_RWAV,              /* contents of RWAR */
    meta_CWAV,              /* contents of CWAR */
    meta_FWAV,              /* contents of FWAR */
    meta_RSTM_SPM,          /* RSTM with 44->22khz hack */
    meta_THP,               /* THP movie files */
    meta_RSTM_shrunken,     /* Atlus' mutant shortened RSTM */
    meta_NDS_SWAV,          /* Asphalt Urban GT 1 & 2 */
    meta_NDS_RRDS,          /* Ridge Racer DS */
    meta_WII_BNS,           /* Wii BNS Banner Sound (similar to RSTM) */
    meta_STX,               /* Pikmin .stx */
    meta_WIIU_BTSND,        /* Wii U Boot Sound */

    meta_ADX_03,            /* CRI ADX "type 03" */
    meta_ADX_04,            /* CRI ADX "type 04" */
    meta_ADX_05,            /* CRI ADX "type 05" */
    meta_AIX,               /* CRI AIX */
    meta_AAX,               /* CRI AAX */
    meta_UTF_DSP,           /* CRI ADPCM_WII, like AAX with DSP */

    meta_NGC_ADPDTK,        /* NGC DTK/ADP (.adp/dkt DTK) [no header_id] */
    meta_RSF,               /* Retro Studios RSF (Metroid Prime .rsf) [no header_id] */
    meta_HALPST,            /* HAL Labs HALPST */
    meta_GCSW,              /* GCSW (PCM) */
    meta_CAF,               /* tri-Crescendo CAF */
    meta_MYSPD,             /* U-Sing .myspd */
    meta_HIS,               /* Her Ineractive .his */
    meta_BNSF,              /* Bandai Namco Sound Format */

    meta_XA,                /* CD-ROM XA */
    meta_PS2_SShd,          /* .ADS with SShd header */
    meta_PS2_NPSF,          /* Namco Production Sound File */
    meta_PS2_RXWS,          /* Sony games (Genji, Okage Shadow King, Arc The Lad Twilight of Spirits) */
    meta_PS2_RAW,           /* RAW Interleaved Format */
    meta_PS2_EXST,          /* Shadow of Colossus EXST */
    meta_PS2_SVAG,          /* Konami SVAG */
    meta_PS_HEADERLESS,     /* headerless PS-ADPCM */
    meta_PS2_MIB_MIH,       /* MIB File + MIH Header*/
    meta_PS2_MIC,           /* KOEI MIC File */
    meta_PS2_VAGi,          /* VAGi Interleaved File */
    meta_PS2_VAGp,          /* VAGp Mono File */
    meta_PS2_pGAV,          /* VAGp with Little Endian Header */
    meta_PSX_GMS,           /* GMS File (used in PS1 & PS2) [no header_id] */
    meta_STR_WAV,           /* Blitz Games STR+WAV files */
    meta_PS2_ILD,           /* ILD File */
    meta_PS2_PNB,           /* PsychoNauts Bgm File */
    meta_VPK,               /* VPK Audio File */
    meta_PS2_BMDX,          /* Beatmania thing */
    meta_PS2_IVB,           /* Langrisser 3 IVB */
    meta_PS2_SND,           /* some Might & Magics SSND header */
    meta_SVS,               /* Square SVS */
    meta_XSS,               /* Dino Crisis 3 */
    meta_SL3,               /* Test Drive Unlimited */
    meta_HGC1,              /* Knights of the Temple 2 */
    meta_AUS,               /* Various Capcom games */
    meta_RWS,               /* RenderWare games (only when using RW Audio middleware) */
    meta_FSB1,              /* FMOD Sample Bank, version 1 */
    meta_FSB2,              /* FMOD Sample Bank, version 2 */
    meta_FSB3,              /* FMOD Sample Bank, version 3.0/3.1 */
    meta_FSB4,              /* FMOD Sample Bank, version 4 */
    meta_FSB5,              /* FMOD Sample Bank, version 5 */
    meta_RWX,               /* Air Force Delta Storm (XBOX) */
    meta_XWB,               /* Microsoft XACT framework (Xbox, X360, Windows) */
    meta_PS2_XA30,          /* Driver - Parallel Lines (PS2) */
    meta_MUSC,              /* Krome PS2 games */
    meta_MUSX_V004,         /* Spyro Games, possibly more */
    meta_MUSX_V005,         /* Spyro Games, possibly more */
    meta_MUSX_V006,         /* Spyro Games, possibly more */
    meta_MUSX_V010,         /* Spyro Games, possibly more */
    meta_MUSX_V201,         /* Sphinx and the cursed Mummy */
    meta_LEG,               /* Legaia 2 [no header_id] */
    meta_FILP,              /* Resident Evil - Dead Aim */
    meta_IKM,               /* Zwei! */
    meta_SFS,               /* Baroque */
    meta_BG00,              /* Ibara, Mushihimesama */
    meta_PS2_RSTM,          /* Midnight Club 3 */
    meta_PS2_KCES,          /* Dance Dance Revolution */
    meta_PS2_DXH,           /* Tokobot Plus - Myteries of the Karakuri */
    meta_VSV,
    meta_SCD_PCM,           /* Lunar - Eternal Blue */
    meta_PS2_PCM,           /* Konami KCEJ East: Ephemeral Fantasia, Yu-Gi-Oh! The Duelists of the Roses, 7 Blades */
    meta_PS2_RKV,           /* Legacy of Kain - Blood Omen 2 (PS2) */
    meta_PS2_VAS,           /* Pro Baseball Spirits 5 */
    meta_PS2_TEC,           /* TECMO badflagged stream */
    meta_PS2_ENTH,          /* Enthusia */
    meta_SDT,               /* Baldur's Gate - Dark Alliance */
    meta_NGC_TYDSP,         /* Ty - The Tasmanian Tiger */
    meta_NGC_SWD,           /* Conflict - Desert Storm 1 & 2 */
    meta_CAPDSP,            /* Capcom DSP Header [no header_id] */
    meta_DC_STR,            /* SEGA Stream Asset Builder */
    meta_DC_STR_V2,         /* variant of SEGA Stream Asset Builder */
    meta_NGC_BH2PCM,        /* Bio Hazard 2 */
    meta_SAT_SAP,           /* Bubble Symphony */
    meta_DC_IDVI,           /* Eldorado Gate */
    meta_KRAW,              /* Geometry Wars - Galaxies */
    meta_PS2_OMU,           /* PS2 Int file with Header */
    meta_PS2_XA2,           /* XG3 Extreme-G Racing */
    meta_NUB_IDSP,          /* Soul Calibur Legends (Wii) */
    meta_IDSP_NL,           /* Mario Strikers Charged (Wii) */
    meta_IDSP_IE,           /* Defencer (GC) */
    meta_SPT_SPD,           /* Various (SPT+SPT DSP) */
    meta_ISH_ISD,           /* Various (ISH+ISD DSP) */
    meta_GSP_GSB,           /* Tecmo games (Super Swing Golf 1 & 2, Quamtum Theory) */
    meta_YDSP,              /* WWE Day of Reckoning */
    meta_FFCC_STR,          /* Final Fantasy: Crystal Chronicles */
    meta_UBI_JADE,          /* Beyond Good & Evil, Rayman Raving Rabbids */
    meta_GCA,               /* Metal Slug Anthology */
    meta_MSVP,              /* Popcap Hits */
    meta_NGC_SSM,           /* Golden Gashbell Full Power */
    meta_PS2_JOE,           /* Wall-E / Pixar games */
    meta_NGC_YMF,           /* WWE WrestleMania X8 */
    meta_SADL,              /* .sad */
    meta_PS2_CCC,           /* Tokyo Xtreme Racer DRIFT 2 */
    meta_FAG,               /* Jackie Chan - Stuntmaster */
    meta_PS2_MIHB,          /* Merged MIH+MIB */
    meta_NGC_PDT,           /* Mario Party 6 */
    meta_DC_ASD,            /* Miss Moonligh */
    meta_NAOMI_SPSD,        /* Guilty Gear X */
    
    meta_RSD2VAG,           /* RSD2VAG */
    meta_RSD2PCMB,          /* RSD2PCMB */
    meta_RSD2XADP,          /* RSD2XADP */
    meta_RSD3VAG,           /* RSD3VAG */
    meta_RSD3GADP,          /* RSD3GADP */
    meta_RSD3PCM,           /* RSD3PCM */
    meta_RSD3PCMB,          /* RSD3PCMB */
    meta_RSD4PCMB,          /* RSD4PCMB */
    meta_RSD4PCM,           /* RSD4PCM */
    meta_RSD4RADP,          /* RSD4RADP */
    meta_RSD4VAG,           /* RSD4VAG */
    meta_RSD6VAG,           /* RSD6VAG */
    meta_RSD6WADP,          /* RSD6WADP */
    meta_RSD6XADP,          /* RSD6XADP */
    meta_RSD6RADP,          /* RSD6RADP */
    meta_RSD6OOGV,          /* RSD6OOGV */
    meta_RSD6XMA,           /* RSD6XMA */
    meta_RSD6AT3P,          /* RSD6AT3+ */
    meta_RSD6WMA,           /* RSD6WMA */

    meta_PS2_ASS,           /* ASS */
    meta_SEG,               /* Eragon */
    meta_NDS_STRM_FFTA2,    /* Final Fantasy Tactics A2 */
    meta_STR_ASR,           /* Donkey Kong Jet Race */
    meta_ZWDSP,             /* Zack and Wiki */
    meta_VGS,               /* Guitar Hero Encore - Rocks the 80s */
    meta_DC_DCSW_DCS,       /* Evil Twin - Cypriens Chronicles (DC) */
    meta_SMP,
    meta_WII_SNG,           /* Excite Trucks */
    meta_MUL,
    meta_SAT_BAKA,          /* Crypt Killer */
    meta_PS2_VSF,           /* Musashi: Samurai Legend */
    meta_PS2_VSF_TTA,       /* Tiny Toon Adventures: Defenders of the Universe */
    meta_ADS,               /* Gauntlet Dark Legends (GC) */
    meta_PS2_SPS,           /* Ape Escape 2 */
    meta_PS2_XA2_RRP,       /* RC Revenge Pro */
    meta_NGC_DSP_KONAMI,    /* Konami DSP header, found in various games */
    meta_UBI_CKD,           /* Ubisoft CKD RIFF header (Rayman Origins Wii) */

    meta_XBOX_WAVM,         /* XBOX WAVM File */
    meta_XBOX_WVS,          /* XBOX WVS */
    meta_NGC_WVS,           /* Metal Arms - Glitch in the System */
    meta_XBOX_MATX,         /* XBOX MATX */
    meta_XBOX_XMU,          /* XBOX XMU */
    meta_XBOX_XVAS,         /* XBOX VAS */
    
    meta_EA_SCHL,           /* Electronic Arts SCHl with variable header */
    meta_EA_SCHL_fixed,     /* Electronic Arts SCHl with fixed header */
    meta_EA_BNK,            /* Electronic Arts BNK */
    meta_EA_1SNH,           /* Electronic Arts 1SNh/EACS */
    meta_EA_EACS,

    meta_RAW,               /* RAW PCM file */

    meta_GENH,              /* generic header */

    meta_AIFC,              /* Audio Interchange File Format AIFF-C */
    meta_AIFF,              /* Audio Interchange File Format */
    meta_STR_SNDS,          /* .str with SNDS blocks and SHDR header */
    meta_WS_AUD,            /* Westwood Studios .aud */
    meta_WS_AUD_old,        /* Westwood Studios .aud, old style */
    meta_RIFF_WAVE,         /* RIFF, for WAVs */
    meta_RIFF_WAVE_POS,     /* .wav + .pos for looping (Ys Complete PC) */
    meta_RIFF_WAVE_labl,    /* RIFF w/ loop Markers in LIST-adtl-labl */
    meta_RIFF_WAVE_smpl,    /* RIFF w/ loop data in smpl chunk */
    meta_RIFF_WAVE_wsmp,    /* RIFF w/ loop data in wsmp chunk */
    meta_RIFF_WAVE_MWV,     /* .mwv RIFF w/ loop data in ctrl chunk pflt */
    meta_RIFX_WAVE,         /* RIFX, for big-endian WAVs */
    meta_RIFX_WAVE_smpl,    /* RIFX w/ loop data in smpl chunk */
    meta_XNB,               /* XNA Game Studio 4.0 */
    meta_PC_MXST,           /* Lego Island MxSt */
    meta_SAB,               /* Worms 4 Mayhem SAB+SOB file */
    meta_NWA,               /* Visual Art's NWA */
    meta_NWA_NWAINFOINI,    /* Visual Art's NWA w/ NWAINFO.INI for looping */
    meta_NWA_GAMEEXEINI,    /* Visual Art's NWA w/ Gameexe.ini for looping */
    meta_SAT_DVI,           /* Konami KCE Nagoya DVI (SAT games) */
    meta_DC_KCEY,           /* Konami KCE Yokohama KCEYCOMP (DC games) */
    meta_ACM,               /* InterPlay ACM header */
    meta_MUS_ACM,           /* MUS playlist of InterPlay ACM files */
    meta_DEC,               /* Falcom PC games (Xanadu Next, Gurumin) */
    meta_VS,                /* Men in Black .vs */
    meta_FFXI_BGW,          /* FFXI (PC) BGW */
    meta_FFXI_SPW,          /* FFXI (PC) SPW */
    meta_STS_WII,           /* Shikigami No Shiro 3 STS Audio File */
    meta_PS2_P2BT,          /* Pop'n'Music 7 Audio File */
    meta_PS2_GBTS,          /* Pop'n'Music 9 Audio File */
    meta_NGC_DSP_IADP,      /* Gamecube Interleave DSP */
    meta_PS2_TK5,           /* Tekken 5 Stream Files */
    meta_PS2_MCG,           /* Gunvari MCG Files (was name .GCM on disk) */
    meta_ZSD,               /* Dragon Booster ZSD */
    meta_REDSPARK,          /* "RedSpark" RSD (MadWorld) */
    meta_IVAUD,             /* .ivaud GTA IV */
    meta_NDS_HWAS,          /* Spider-Man 3, Tony Hawk's Downhill Jam, possibly more... */
    meta_NGC_LPS,           /* Rave Master (Groove Adventure Rave)(GC) */
    meta_NAOMI_ADPCM,       /* NAOMI/NAOMI2 ARcade games */
    meta_SD9,               /* beatmaniaIIDX16 - EMPRESS (Arcade) */
    meta_2DX9,              /* beatmaniaIIDX16 - EMPRESS (Arcade) */
    meta_PS2_VGV,           /* Rune: Viking Warlord */
    meta_NGC_GCUB,          /* Sega Soccer Slam */
    meta_MAXIS_XA,          /* Sim City 3000 (PC) */
    meta_NGC_SCK_DSP,       /* Scorpion King (NGC) */
    meta_CAFF,              /* iPhone .caf */
    meta_EXAKT_SC,          /* Activision EXAKT .SC (PS2) */
    meta_WII_WAS,           /* DiRT 2 (WII) */
    meta_PONA_3DO,          /* Policenauts (3DO) */
    meta_PONA_PSX,          /* Policenauts (PSX) */
    meta_XBOX_HLWAV,        /* Half Life 2 (XBOX) */
    meta_PS2_AST,           /* Some KOEI game (PS2) */
    meta_DMSG,              /* Nightcaster II - Equinox (XBOX) */
    meta_NGC_DSP_AAAP,      /* Turok: Evolution (NGC), Vexx (NGC) */
    meta_PS2_STER,          /* Juuni Kokuki: Kakukaku Taru Ou Michi Beni Midori no Uka */
    meta_PS2_WB,            /* Shooting Love. ~TRIZEAL~ */
    meta_S14,               /* raw Siren 14, 24kbit mono */
    meta_SSS,               /* raw Siren 14, 48kbit stereo */
    meta_PS2_GCM,           /* NamCollection */
    meta_PS2_SMPL,          /* Homura */
    meta_PS2_MSA,           /* Psyvariar -Complete Edition- */
    meta_PS2_VOI,           /* RAW Danger (Zettaizetsumei Toshi 2 - Itetsuita Kiokutachi) [PS2] */
    meta_P3D,               /* Prototype P3D */
    meta_PS2_TK1,           /* Tekken (NamCollection) */
    meta_NGC_RKV,           /* Legacy of Kain - Blood Omen 2 (GC) */
    meta_DSP_DDSP,          /* Various (2 dsp files stuck together */
    meta_NGC_DSP_MPDS,      /* Big Air Freestyle, Terminator 3 */
    meta_DSP_STR_IG,        /* Micro Machines, Superman Superman: Shadow of Apokolis */
    meta_EA_SWVR,           /* Future Cop L.A.P.D., Freekstyle */
    meta_PS2_B1S,           /* 7 Wonders of the ancient world */
    meta_PS2_WAD,           /* The golden Compass */
    meta_DSP_XIII,          /* XIII, possibly more (Ubisoft header???) */
    meta_DSP_CABELAS,       /* Cabelas games */
    meta_PS2_ADM,           /* Dragon Quest V (PS2) */
    meta_PS2_LPCM,          /* Ah! My Goddess */
    meta_DSP_BDSP,          /* Ah! My Goddess */
    meta_PS2_VMS,           /* Autobahn Raser - Police Madness */
    meta_XAU,               /* XPEC Entertainment (Beat Down (PS2 Xbox), Spectral Force Chronicle (PS2)) */
    meta_GH3_BAR,           /* Guitar Hero III Mobile .bar */
    meta_FFW,               /* Freedom Fighters [NGC] */
    meta_DSP_DSPW,          /* Sengoku Basara 3 [WII] */
    meta_PS2_JSTM,          /* Tantei Jinguji Saburo - Kind of Blue (PS2) */
    meta_SQEX_SCD,          /* Square-Enix SCD */
    meta_NGC_NST_DSP,       /* Animaniacs [NGC] */
    meta_BAF,               /* Bizarre Creations (Blur, James Bond) */
    meta_XVAG,              /* Ratchet & Clank Future: Quest for Booty (PS3) */
    meta_PS3_CPS,           /* Eternal Sonata (PS3) */
    meta_PS3_MSF,           /* MSF header */
    meta_NUB_VAG,           /* Namco VAG from NUB archives */
    meta_PS3_PAST,          /* Bakugan Battle Brawlers (PS3) */
    meta_SGXD,              /* Sony: Folklore, Genji, Tokyo Jungle (PS3), Brave Story, Kurohyo (PSP) */
    meta_NGCA,              /* GoldenEye 007 (Wii) */
    meta_WII_RAS,           /* Donkey Kong Country Returns (Wii) */
    meta_PS2_SPM,           /* Lethal Skies Elite Pilot: Team SW */
    meta_X360_TRA,          /* Def Jam Rapstar */
    meta_PS2_VGS,           /* Princess Soft PS2 games */
    meta_PS2_IAB,           /* Ueki no Housoku - Taosu ze Robert Juudan!! (PS2) */
    meta_VS_STR,            /* The Bouncer */
    meta_LSF_N1NJ4N,        /* .lsf n1nj4n Fastlane Street Racing (iPhone) */
    meta_VAWX,              /* feelplus: No More Heroes Heroes Paradise, Moon Diver */
    meta_PC_SNDS,           /* Incredibles PC .snds */
    meta_PS2_WMUS,          /* The Warriors (PS2) */
    meta_HYPERSCAN_KVAG,    /* Hyperscan KVAG/BVG */
    meta_IOS_PSND,          /* Crash Bandicoot Nitro Kart 2 (iOS) */
    meta_BOS_ADP,           /* ADP! (Balls of Steel, PC) */
    meta_OTNS_ADP,          /* Omikron: The Nomad Soul .adp (PC/DC) */
    meta_EB_SFX,            /* Excitebots .sfx */
    meta_EB_SF0,            /* Excitebots .sf0 */
    meta_PS2_MTAF,          /* Metal Gear Solid 3 MTAF */
    meta_PS2_VAG1,          /* Metal Gear Solid 3 VAG1 */
    meta_PS2_VAG2,          /* Metal Gear Solid 3 VAG2 */
    meta_TUN,               /* LEGO Racers (PC) */
    meta_WPD,               /* Shuffle! (PC) */
    meta_MN_STR,            /* Mini Ninjas (PC/PS3/WII) */
    meta_MSS,               /* Guerilla: ShellShock Nam '67 (PS2/Xbox), Killzone (PS2) */
    meta_PS2_HSF,           /* Lowrider (PS2) */
    meta_PS3_IVAG,          /* Interleaved VAG files (PS3) */
    meta_PS2_2PFS,          /* Konami: Mahoromatic: Moetto - KiraKira Maid-San, GANTZ (PS2) */
    meta_PS2_VBK,           /* Disney's Stitch - Experiment 626 */
    meta_OTM,               /* Otomedius (Arcade) */
    meta_CSTM,              /* Nintendo 3DS CSTM (Century Stream) */
    meta_FSTM,              /* Nintendo Wii U FSTM (caFe? Stream) */
    meta_IDSP_NUS3,         /* Namco 3DS/Wii U IDSP */
    meta_KT_WIIBGM,         /* Koei Tecmo WiiBGM */
    meta_KTSS,              /* Koei Tecmo Nintendo Stream (KNS) */
    meta_MCA,               /* Capcom MCA "MADP" */
    meta_XB3D_ADX,          /* Xenoblade Chronicles 3D ADX */
    meta_HCA,               /* CRI HCA */
    meta_PS2_SVAG_SNK,      /* SNK PS2 SVAG */
    meta_PS2_VDS_VDM,       /* Graffiti Kingdom */
    meta_FFMPEG,            /* any file supported by FFmpeg */
    meta_X360_CXS,          /* Eternal Sonata (Xbox 360) */
    meta_AKB,               /* SQEX iOS */
    meta_NUB_XMA,           /* Namco XMA from NUB archives */
    meta_X360_PASX,         /* Namco PASX (Soul Calibur II HD X360) */
    meta_XMA_RIFF,          /* Microsoft RIFF XMA */
    meta_X360_AST,          /* Dead Rising (X360) */
    meta_WWISE_RIFF,        /* Audiokinetic Wwise RIFF/RIFX */
    meta_UBI_RAKI,          /* Ubisoft RAKI header (Rayman Legends, Just Dance 2017) */
    meta_SXD,               /* Sony SXD (Gravity Rush, Freedom Wars PSV) */
    meta_OGL,               /* Shin'en Wii/WiiU (Jett Rocket (Wii), FAST Racing NEO (WiiU)) */
    meta_MC3,               /* Paradigm games (T3 PS2, MX Rider PS2, MI: Operation Surma PS2) */
    meta_GTD,               /* Knights Contract (X360/PS3), Valhalla Knights 3 (PSV) */
    meta_TA_AAC_X360,       /* tri-Ace AAC (Star Ocean 4, End of Eternity, Infinite Undiscovery) */
    meta_TA_AAC_PS3,        /* tri-Ace AAC (Star Ocean International, Resonance of Fate) */
    meta_TA_AAC_MOBILE,     /* tri-Ace AAC (Star Ocean Anamnesis, Heaven x Inferno) */
    meta_PS3_MTA2,          /* Metal Gear Solid 4 MTA2 */
    meta_NGC_ULW,           /* Burnout 1 (GC only) */
    meta_PC_XA30,           /* Driver - Parallel Lines (PC) */
    meta_WII_04SW,          /* Driver - Parallel Lines (Wii) */
    meta_TXTH,              /* generic text header */
    meta_SK_AUD,            /* Silicon Knights .AUD (Eternal Darkness GC) */
    meta_AHX,               /* CRI AHX header */
    meta_STM,               /* Angel Studios/Rockstar San Diego Games */
    meta_BINK,              /* RAD Game Tools BINK audio/video */
    meta_EA_SNU,            /* Electronic Arts SNU (Dead Space) */
    meta_AWC,               /* Rockstar AWC (GTA5, RDR) */
    meta_OPUS,              /* Nintendo Opus [Lego City Undercover (Switch)] */
    meta_PC_AL2,            /* Conquest of Elysium 3 (PC) */
    meta_PC_AST,            /* Dead Rising (PC) */
    meta_NAAC,              /* Namco AAC (3DS) */
    meta_UBI_SB,            /* Ubisoft banks */
    meta_EZW,               /* EZ2DJ (Arcade) EZWAV */
    meta_VXN,               /* Gameloft mobile games */
    meta_EA_SNR_SNS,        /* Electronic Arts SNR+SNS (Burnout Paradise) */
    meta_EA_SPS,            /* Electronic Arts SPS (Burnout Crash) */
    meta_NGC_VID1,          /* Neversoft .ogg (Gun GC) */
    meta_PC_FLX,            /* Ultima IX PC */
    meta_MOGG,              /* Harmonix Music Systems MOGG Vorbis */
    meta_OGG_VORBIS,        /* Ogg Vorbis */
    meta_OGG_SLI,           /* Ogg Vorbis file w/ companion .sli for looping */
    meta_OPUS_SLI,          /* Ogg Opus file w/ companion .sli for looping */
    meta_OGG_SFL,           /* Ogg Vorbis file w/ .sfl (RIFF SFPL) for looping */
    meta_OGG_KOVS,          /* Ogg Vorbis with header and encryption (Koei Tecmo Games) */
    meta_OGG_encrypted,     /* Ogg Vorbis with encryption */
    meta_KMA9,              /* Koei Tecmo [Nobunaga no Yabou - Souzou (Vita)] */
    meta_XWC,               /* Starbreeze games */
    meta_SQEX_SAB,          /* Square-Enix newest middleware (sound) */
    meta_SQEX_MAB,          /* Square-Enix newest middleware (music) */
    meta_WAF,               /* KID WAF [Ever 17 (PC)] */
    meta_WAVE,              /* EngineBlack games [Mighty Switch Force! (3DS)] */
    meta_WAVE_segmented,    /* EngineBlack games, segmented [Shantae and the Pirate's Curse (PC)] */
    meta_SMV,               /* Cho Aniki Zero (PSP) */
    meta_NXAP,              /* Nex Entertainment games [Time Crisis 4 (PS3), Time Crisis Razing Storm (PS3)] */
    meta_EA_WVE_AU00,       /* Electronic Arts PS movies [Future Cop - L.A.P.D. (PS), Supercross 2000 (PS)] */
    meta_EA_WVE_AD10,       /* Electronic Arts PS movies [Wing Commander 3/4 (PS)] */
    meta_STHD,              /* STHD .stx [Kakuto Chojin (Xbox)] */
    meta_MP4,               /* MP4/AAC */
    meta_PCM_SRE,           /* .PCM+SRE [Viewtiful Joe (PS2)] */
    meta_DSP_MCADPCM,       /* Skyrim (Switch) */
    meta_UBI_LYN,           /* Ubisoft LyN engine [The Adventures of Tintin (multi)] */
    meta_MSB_MSH,           /* sfx companion of MIH+MIB */
    meta_TXTP,              /* generic text playlist */
    meta_SMC_SMH,           /* Wangan Midnight (System 246) */
    meta_PPST,              /* PPST [Parappa the Rapper (PSP)] */
    meta_OPUS_PPP,          /* .at9 Opus [Penny-Punching Princess (Switch)] */
    meta_UBI_BAO,           /* Ubisoft BAO */
    meta_DSP_SWITCH_AUDIO,  /* Gal Gun 2 (Switch) */
    meta_TA_AAC_VITA,       /* tri-Ace AAC (Judas Code) */
    meta_H4M,               /* Hudson HVQM4 video [Resident Evil 0 (GC), Tales of Symphonia (GC)] */
    meta_ASF,               /* Argonaut ASF [Croc 2 (PC)] */
    meta_XMD,               /* Konami XMD [Silent Hill 4 (Xbox), Castlevania: Curse of Darkness (Xbox)] */
    meta_CKS,               /* Cricket Audio stream [Part Time UFO (Android), Mega Man 1-6 (Android)] */
    meta_CKB,               /* Cricket Audio bank [Fire Emblem Heroes (Android), Mega Man 1-6 (Android)] */
    meta_WV6,               /* Gorilla Systems PC games */
    meta_WAVEBATCH,         /* Firebrand Games */
    meta_HD3_BD3,           /* Sony PS3 bank */
    meta_BNK_SONY,          /* Sony Scream Tool bank */
    meta_SCD_SSCF,          /* Square Enix SCD old version */
    meta_DSP_VAG,           /* Penny-Punching Princess (Switch) sfx */
    meta_DSP_ITL,           /* Charinko Hero (GC) */
    meta_A2M,               /* Scooby-Doo! Unmasked (PS2) */
    meta_AHV,               /* Headhunter (PS2) */
    meta_MSV,               /* Fight Club (PS2) */
    meta_SDF,
    meta_SVG,               /* Hunter - The Reckoning - Wayward (PS2) */
    meta_VIS,               /* AirForce Delta Strike (PS2) */
    meta_VAI,               /* Ratatouille (GC) */
    meta_AIF_ASOBO,         /* Ratatouille (PC) */
    meta_AO,                /* Cloudphobia (PC) */
    meta_APC,               /* MegaRace 3 (PC) */
    meta_WV2,               /* Slave Zero (PC) */
    meta_XAU_KONAMI,        /* Yu-Gi-Oh - The Dawn of Destiny (Xbox) */
    meta_DERF,              /* Stupid Invaders (PC) */
    meta_UTK,
    meta_NXA,
    meta_ADPCM_CAPCOM,
    meta_UE4OPUS,
    meta_XWMA,
    meta_VA3,               /* DDR Supernova 2 AC */
    meta_XOPUS,
    meta_VS_SQUARE,
    meta_NWAV,
    meta_XPCM,
    meta_MSF_TAMASOFT,
    meta_XPS_DAT,
    meta_ZSND,
    meta_DSP_ADPCMX,
    meta_OGG_OPUS,
    meta_IMC,
    meta_GIN,

} meta_t;


/* info for a single vgmstream channel */
typedef struct {
    STREAMFILE * streamfile; /* file used by this channel */
    off_t channel_start_offset; /* where data for this channel begins */
    off_t offset;           /* current location in the file */

    off_t frame_header_offset;  /* offset of the current frame header (for WS) */
    int samples_left_in_frame;  /* for WS */

    /* format specific */

    /* adpcm */
    int16_t adpcm_coef[16]; /* for formats with decode coefficients built in */
    int32_t adpcm_coef_3by32[0x60];     /* for Level-5 0x555 */
    union {
        int16_t adpcm_history1_16;  /* previous sample */
        int32_t adpcm_history1_32;
    };
    union {
        int16_t adpcm_history2_16;  /* previous previous sample */
        int32_t adpcm_history2_32;
    };
    union {
        int16_t adpcm_history3_16;
        int32_t adpcm_history3_32;
    };
    union {
        int16_t adpcm_history4_16;
        int32_t adpcm_history4_32;
    };

    double adpcm_history1_double;
    double adpcm_history2_double;

    int adpcm_step_index;       /* for IMA */
    int adpcm_scale;            /* for MS ADPCM */

    /* state for G.721 decoder, sort of big but we might as well keep it around */
    struct g72x_state g72x_state;

    /* ADX encryption */
    int adx_channels;
    uint16_t adx_xor;
    uint16_t adx_mult;
    uint16_t adx_add;

} VGMSTREAMCHANNEL;

/* main vgmstream info */
typedef struct {
    /* basics */
    int32_t num_samples;            /* the actual max number of samples */
    int32_t sample_rate;            /* sample rate in Hz */
    int channels;                   /* number of channels */
    coding_t coding_type;           /* type of encoding */
    layout_t layout_type;           /* type of layout */
    meta_t meta_type;               /* type of metadata */

    /* looping */
    int loop_flag;                  /* is this stream looped? */
    int32_t loop_start_sample;      /* first sample of the loop (included in the loop) */
    int32_t loop_end_sample;        /* last sample of the loop (not included in the loop) */

    /* layouts/block */
    size_t interleave_block_size;   /* interleave, or block/frame size (depending on the codec) */
    size_t interleave_last_block_size; /* smaller interleave for last block */

    /* subsongs */
    int num_streams;                /* for multi-stream formats (0=not set/one stream, 1=one stream) */
    int stream_index;               /* selected subsong (also 1-based) */
    size_t stream_size;             /* info to properly calculate bitrate in case of subsongs */
    char stream_name[STREAM_NAME_SIZE]; /* name of the current stream (info), if the file stores it and it's filled */

    /* config */
    int allow_dual_stereo;          /* search for dual stereo (file_L.ext + file_R.ext = single stereo file) */
    uint32_t channel_mask;          /* to silence crossfading subsongs/layers */
    int channel_mappings_on;        /* channel mappings are active */
    int channel_mappings[32];       /* swap channel "i" with "[i]" */
    /* config requests, players must read and honor these values */
    /* (ideally internally would work as a player, but for now player must do it manually) */
    double config_loop_count;
    double config_fade_time;
    double config_fade_delay;
    int config_ignore_loop;
    int config_force_loop;
    int config_ignore_fade;


    /* channel state */
    VGMSTREAMCHANNEL * ch;          /* pointer to array of channels */
    VGMSTREAMCHANNEL * start_ch;    /* copies of channel status as they were at the beginning of the stream */
    VGMSTREAMCHANNEL * loop_ch;     /* copies of channel status as they were at the loop point */

    /* layout/block state */
    size_t full_block_size;         /* actual data size of an entire block (ie. may be fixed, include padding/headers, etc) */
    int32_t current_sample;         /* number of samples we've passed (for loop detection) */
    int32_t samples_into_block;     /* number of samples into the current block/interleave/segment/etc */
    off_t current_block_offset;     /* start of this block (offset of block header) */
    size_t current_block_size;      /* size in usable bytes of the block we're in now (used to calculate num_samples per block) */
    size_t current_block_samples;   /* size in samples of the block we're in now (used over current_block_size if possible) */
    off_t next_block_offset;        /* offset of header of the next block */
    /* layout/block loop state */
    int32_t loop_sample;            /* saved from current_sample, should be loop_start_sample... */
    int32_t loop_samples_into_block;/* saved from samples_into_block */
    off_t loop_block_offset;        /* saved from current_block_offset */
    size_t loop_block_size;         /* saved from current_block_size */
    size_t loop_block_samples;      /* saved from current_block_samples */
    off_t loop_next_block_offset;   /* saved from next_block_offset */

    /* loop state */
    int hit_loop;                   /* have we seen the loop yet? */
    int loop_count;                 /* counter of complete loops (1=looped once) */
    int loop_target;                /* max loops before continuing with the stream end (loops forever if not set) */

    /* decoder specific */
    int codec_endian;               /* little/big endian marker; name is left vague but usually means big endian */
    int codec_config;               /* flags for codecs or layouts with minor variations; meaning is up to the codec */

    int32_t ws_output_size;         /* WS ADPCM: output bytes for this block */

    void * start_vgmstream;         /* a copy of the VGMSTREAM as it was at the beginning of the stream (for custom layouts) */

    /* Data the codec needs for the whole stream. This is for codecs too
     * different from vgmstream's structure to be reasonably shoehorned into
     * using the ch structures.
     * Note also that support must be added for resetting, looping and
     * closing for every codec that uses this, as it will not be handled. */
    void * codec_data;
    /* Same, for special layouts. layout_data + codec_data may exist at the same time. */
    void * layout_data;
} VGMSTREAM;

#ifdef VGM_USE_VORBIS
/* Ogg with Vorbis */
typedef struct {
    STREAMFILE *streamfile;
    ogg_int64_t start; /* file offset where the Ogg starts */
    ogg_int64_t offset; /* virtual offset, from 0 to size */
    ogg_int64_t size; /* virtual size of the Ogg */

    /* decryption setup */
    void (*decryption_callback)(void *ptr, size_t size, size_t nmemb, void *datasource);
    uint8_t scd_xor;
    off_t scd_xor_length;
    uint32_t xor_value;

} ogg_vorbis_streamfile;

typedef struct {
    OggVorbis_File ogg_vorbis_file;
    int bitstream;

    ogg_vorbis_streamfile ov_streamfile;
} ogg_vorbis_codec_data;


/* custom Vorbis modes */
typedef enum {
    VORBIS_FSB,         /* FMOD FSB: simplified/external setup packets, custom packet headers */
    VORBIS_WWISE,       /* Wwise WEM: many variations (custom setup, headers and data) */
    VORBIS_OGL,         /* Shin'en OGL: custom packet headers */
    VORBIS_SK,          /* Silicon Knights AUD: "OggS" replaced by "SK" */
    VORBIS_VID1,        /* Neversoft VID1: custom packet blocks/headers */
} vorbis_custom_t;

/* config for Wwise Vorbis (3 types for flexibility though not all combinations exist) */
typedef enum { WWV_HEADER_TRIAD, WWV_FULL_SETUP, WWV_INLINE_CODEBOOKS, WWV_EXTERNAL_CODEBOOKS, WWV_AOTUV603_CODEBOOKS } wwise_setup_t;
typedef enum { WWV_TYPE_8, WWV_TYPE_6, WWV_TYPE_2 } wwise_header_t;
typedef enum { WWV_STANDARD, WWV_MODIFIED } wwise_packet_t;

typedef struct {
    /* to reconstruct init packets */
    int channels;
    int sample_rate;
    int blocksize_0_exp;
    int blocksize_1_exp;

    uint32_t setup_id; /* external setup */
    int big_endian; /* flag */

    /* Wwise Vorbis config */
    wwise_setup_t setup_type;
    wwise_header_t header_type;
    wwise_packet_t packet_type;

    /* output (kinda ugly here but to simplify) */
    off_t data_start_offset;

} vorbis_custom_config;

/* custom Vorbis without Ogg layer */
typedef struct {
    vorbis_info vi;             /* stream settings */
    vorbis_comment vc;          /* stream comments */
    vorbis_dsp_state vd;        /* decoder global state */
    vorbis_block vb;            /* decoder local state */
    ogg_packet op;              /* fake packet for internal use */

    uint8_t * buffer;           /* internal raw data buffer */
    size_t buffer_size;

    size_t samples_to_discard;  /* for looping purposes */
    int samples_full;           /* flag, samples available in vorbis buffers */

    vorbis_custom_t type;        /* Vorbis subtype */
    vorbis_custom_config config; /* config depending on the mode */

    /* Wwise Vorbis: saved data to reconstruct modified packets */
    uint8_t mode_blockflag[64+1];   /* max 6b+1; flags 'n stuff */
    int mode_bits;                  /* bits to store mode_number */
    uint8_t prev_blockflag;         /* blockflag in the last decoded packet */
    /* Ogg-style Vorbis: packet within a page */
    int current_packet;
    /* reference for page/blocks */
    off_t block_offset;
    size_t block_size;

    int prev_block_samples;     /* count for optimization */

} vorbis_custom_codec_data;
#endif


#ifdef VGM_USE_MPEG
/* Custom MPEG modes, mostly differing in the data layout */
typedef enum {
    MPEG_STANDARD,          /* 1 stream */
    MPEG_AHX,               /* 1 stream with false frame headers */
    MPEG_XVAG,              /* N streams of fixed interleave (frame-aligned, several data-frames of fixed size) */
    MPEG_FSB,               /* N streams of 1 data-frame+padding (=interleave) */
    MPEG_P3D,               /* N streams of fixed interleave (not frame-aligned) */
    MPEG_SCD,               /* N streams of fixed interleave (not frame-aligned) */
    MPEG_EA,                /* 1 stream (maybe N streams in absolute offsets?) */
    MPEG_EAL31,             /* EALayer3 v1 (SCHl), custom frames with v1 header */
    MPEG_EAL31b,            /* EALayer3 v1 (SNS), custom frames with v1 header + minor changes */
    MPEG_EAL32P,            /* EALayer3 v2 "PCM", custom frames with v2 header + bigger PCM blocks? */
    MPEG_EAL32S,            /* EALayer3 v2 "Spike", custom frames with v2 header + smaller PCM blocks? */
    MPEG_LYN,               /* N streams of fixed interleave */
    MPEG_AWC,               /* N streams in block layout (music) or absolute offsets (sfx) */
    MPEG_EAMP3              /* custom frame header + MPEG frame + PCM blocks */
} mpeg_custom_t;

/* config for the above modes */
typedef struct {
    int channels; /* max channels */
    int fsb_padding; /* fsb padding mode */
    int chunk_size; /* size of a data portion */
    int data_size; /* playable size */
    int interleave; /* size of stream interleave */
    int encryption; /* encryption mode */
    int big_endian;
    /* for AHX */
    int cri_type;
    uint16_t cri_key1;
    uint16_t cri_key2;
    uint16_t cri_key3;
} mpeg_custom_config;

/* represents a single MPEG stream */
typedef struct {
    /* per stream as sometimes mpg123 must be fed in passes if data is big enough (ex. EALayer3 multichannel) */
    uint8_t *buffer; /* raw data buffer */
    size_t buffer_size;
    size_t bytes_in_buffer;
    int buffer_full; /* raw buffer has been filled */
    int buffer_used; /* raw buffer has been fed to the decoder */
    mpg123_handle *m; /* MPEG decoder */

    uint8_t *output_buffer; /* decoded samples from this stream (in bytes for mpg123) */
    size_t output_buffer_size;
    size_t samples_filled; /* data in the buffer (in samples) */
    size_t samples_used; /* data extracted from the buffer */

    size_t current_size_count; /* data read (if the parser needs to know) */
    size_t current_size_target; /* max data, until something happens */
    size_t decode_to_discard;  /* discard from this stream only (for EALayer3 or AWC) */

} mpeg_custom_stream;

typedef struct {
    /* regular/single MPEG internals */
    uint8_t *buffer; /* raw data buffer */
    size_t buffer_size;
    size_t bytes_in_buffer;
    int buffer_full; /* raw buffer has been filled */
    int buffer_used; /* raw buffer has been fed to the decoder */
    mpg123_handle *m; /* MPEG decoder */
    struct mpg123_frameinfo mi; /* start info, so it's available even when resetting */

    /* for internal use, assumed to be constant for all frames */
    int channels_per_frame;
    int samples_per_frame;
    /* for some calcs */
    int bitrate_per_frame;
    int sample_rate_per_frame;

    /* custom MPEG internals */
    int custom; /* flag */
    mpeg_custom_t type; /* mpeg subtype */
    mpeg_custom_config config; /* config depending on the mode */

    size_t default_buffer_size;
    mpeg_custom_stream **streams; /* array of MPEG streams (ex. 2ch+2ch) */
    size_t streams_size;

    size_t skip_samples; /* base encoder delay */
    size_t samples_to_discard; /* for custom mpeg looping */

} mpeg_codec_data;
#endif

#ifdef VGM_USE_G7221
typedef struct {
    sample buffer[640];
    g7221_handle *handle;
} g7221_codec_data;
#endif

#ifdef VGM_USE_G719
typedef struct {
   sample buffer[960];
   void *handle;
} g719_codec_data;
#endif

#ifdef VGM_USE_MAIATRAC3PLUS
typedef struct {
    sample *buffer;
    int channels;
    int samples_discard;
    void *handle;
} maiatrac3plus_codec_data;
#endif

#ifdef VGM_USE_ATRAC9
/* ATRAC9 config */
typedef struct {
    int channels;           /* to detect weird multichannel */
    uint32_t config_data;   /* ATRAC9 config header */
    int encoder_delay;      /* initial samples to discard */
} atrac9_config;
typedef struct atrac9_codec_data atrac9_codec_data;
#endif

#ifdef VGM_USE_CELT
typedef enum { CELT_0_06_1,CELT_0_11_0} celt_lib_t;
typedef struct celt_codec_data celt_codec_data;
#endif

/* libacm interface */
typedef struct {
    STREAMFILE *streamfile;
    void *handle;
    void *io_config;
} acm_codec_data;

#define AIX_BUFFER_SIZE 0x1000
/* AIXery */
typedef struct {
    sample buffer[AIX_BUFFER_SIZE];
    int segment_count;
    int stream_count;
    int current_segment;
    /* one per segment */
    int32_t *sample_counts;
    /* organized like:
     * segment1_stream1, segment1_stream2, segment2_stream1, segment2_stream2*/
    VGMSTREAM **adxs;
} aix_codec_data;

/* for files made of "vertical" segments, one per section of a song (using a complete sub-VGMSTREAM) */
typedef struct {
    int segment_count;
    VGMSTREAM **segments;
    int current_segment;
} segmented_layout_data;

/* for files made of "horizontal" layers, one per group of channels (using a complete sub-VGMSTREAM) */
typedef struct {
    int layer_count;
    VGMSTREAM **layers;
} layered_layout_data;

/* for compressed NWA */
typedef struct {
    NWAData *nwa;
} nwa_codec_data;


typedef struct {
    STREAMFILE *streamfile;
    clHCA_stInfo info;

    signed short *sample_buffer;
    size_t samples_filled;
    size_t samples_consumed;
    size_t samples_to_discard;

    void* data_buffer;

    unsigned int current_block;

    void* handle;
} hca_codec_data;

#ifdef VGM_USE_FFMPEG
typedef struct {
    /*** IO internals ***/
    STREAMFILE *streamfile;

    uint64_t start;             // absolute start within the streamfile
    uint64_t offset;            // absolute offset within the streamfile
    uint64_t size;              // max size within the streamfile
    uint64_t logical_offset;    // computed offset FFmpeg sees (including fake header)
    uint64_t logical_size;      // computed size FFmpeg sees (including fake header)
    
    uint64_t header_size;       // fake header (parseable by FFmpeg) prepended on reads
    uint8_t *header_insert_block; // fake header data (ie. RIFF)

    /*** "public" API (read-only) ***/
    // stream info
    int channels;
    int bitsPerSample;
    int floatingPoint;
    int sampleRate;
    int bitrate;
    // extra info: 0 if unknown or not fixed
    int64_t totalSamples; // estimated count (may not be accurate for some demuxers)
    int64_t blockAlign; // coded block of bytes, counting channels (the block can be joint stereo)
    int64_t frameSize; // decoded samples per block
    int64_t skipSamples; // number of start samples that will be skipped (encoder delay), for looping adjustments
    int streamCount; // number of FFmpeg audio streams
    
    /*** internal state ***/
    // Intermediate byte buffer
    uint8_t *sampleBuffer;
    // max samples we can held (can be less or more than frameSize)
    size_t sampleBufferBlock;
    
    // FFmpeg context used for metadata
    AVCodec *codec;
    
    // FFmpeg decoder state
    unsigned char *buffer;
    AVIOContext *ioCtx;
    int streamIndex;
    AVFormatContext *formatCtx;
    AVCodecContext *codecCtx;
    AVFrame *lastDecodedFrame;
    AVPacket *lastReadPacket;
    int bytesConsumedFromDecodedFrame;
    int readNextPacket;
    int endOfStream;
    int endOfAudio;
    int skipSamplesSet; // flag to know skip samples were manually added from vgmstream
    
    // Seeking is not ideal, so rollback is necessary
    int samplesToDiscard;


} ffmpeg_codec_data;
#endif

#ifdef VGM_USE_MP4V2
typedef struct {
    STREAMFILE *streamfile;
    uint64_t start;
    uint64_t offset;
    uint64_t size;
} mp4_streamfile;

#ifdef VGM_USE_FDKAAC
typedef struct {
    mp4_streamfile if_file;
    MP4FileHandle h_mp4file;
    MP4TrackId track_id;
    unsigned long sampleId, numSamples;
    UINT codec_init_data_size;
    HANDLE_AACDECODER h_aacdecoder;
    unsigned int sample_ptr, samples_per_frame, samples_discard;
    INT_PCM sample_buffer[( (6) * (2048)*4 )];
} mp4_aac_codec_data;
#endif
#endif

typedef struct ea_mt_codec_data ea_mt_codec_data;


#if 0
//possible future public/opaque API

/* define standard C param call and name mangling (to avoid __stdcall / .defs) */
#define VGMSTREAM_CALL __cdecl //needed?

/* define external function types (during compilation) */
#if defined(VGMSTREAM_EXPORT)
    #define VGMSTREAM_API __declspec(dllexport) /* when exporting/creating vgmstream DLL */
#elif defined(VGMSTREAM_IMPORT)
    #define VGMSTREAM_API __declspec(dllimport) /* when importing/linking vgmstream DLL */
#else
    #define VGMSTREAM_API /* nothing, internal/default */
#endif

//VGMSTREAM_API void VGMSTREAM_CALL vgmstream_function(void);

//info for opaque VGMSTREAM
typedef struct {
    const int channels;
    const int sample_rate;
    const int num_samples;
    const int loop_start_sample;
    const int loop_end_sample;
    const int loop_flag;
    const int num_streams;
    const int current_sample;
    const int average_bitrate;
} VGMSTREAM_INFO;
void vgmstream_get_info(VGMSTREAM* vgmstream, VGMSTREAM_INFO *vgmstream_info);

//or maybe
enum vgmstream_value_t { VGMSTREAM_CHANNELS, VGMSTREAM_CURRENT_SAMPLE, ... };
int vgmstream_get_info(VGMSTREAM* vgmstream, vgmstream_value_t type);
// or
int vgmstream_get_current_sample(VGMSTREAM* vgmstream);

#endif

/* -------------------------------------------------------------------------*/
/* vgmstream "public" API                                                   */
/* -------------------------------------------------------------------------*/

/* do format detection, return pointer to a usable VGMSTREAM, or NULL on failure */
VGMSTREAM * init_vgmstream(const char * const filename);

/* init with custom IO via streamfile */
VGMSTREAM * init_vgmstream_from_STREAMFILE(STREAMFILE *streamFile);

/* reset a VGMSTREAM to start of stream */
void reset_vgmstream(VGMSTREAM * vgmstream);

/* close an open vgmstream */
void close_vgmstream(VGMSTREAM * vgmstream);

/* calculate the number of samples to be played based on looping parameters */
int32_t get_vgmstream_play_samples(double looptimes, double fadeseconds, double fadedelayseconds, VGMSTREAM * vgmstream);

/* Decode data into sample buffer */
void render_vgmstream(sample * buffer, int32_t sample_count, VGMSTREAM * vgmstream);

/* Write a description of the stream into array pointed by desc, which must be length bytes long.
 * Will always be null-terminated if length > 0 */
void describe_vgmstream(VGMSTREAM * vgmstream, char * desc, int length);

/* Return the average bitrate in bps of all unique files contained within this stream. */
int get_vgmstream_average_bitrate(VGMSTREAM * vgmstream);

/* List supported formats and return elements in the list, for plugins that need to know.
 * The list disables some common formats that may conflict (.wav, .ogg, etc). */
const char ** vgmstream_get_formats(size_t * size);

/* Force enable/disable internal looping. Should be done before playing anything,
 * and not all codecs support arbitrary loop values ATM. */
void vgmstream_force_loop(VGMSTREAM* vgmstream, int loop_flag, int loop_start_sample, int loop_end_sample);

/* Set number of max loops to do, then play up to stream end (for songs with proper endings) */
void vgmstream_set_loop_target(VGMSTREAM* vgmstream, int loop_target);

/* -------------------------------------------------------------------------*/
/* vgmstream "private" API                                                  */
/* -------------------------------------------------------------------------*/

/* Allocate memory and setup a VGMSTREAM */
VGMSTREAM * allocate_vgmstream(int channel_count, int looped);

/* Get the number of samples of a single frame (smallest self-contained sample group, 1/N channels) */
int get_vgmstream_samples_per_frame(VGMSTREAM * vgmstream);
/* Get the number of bytes of a single frame (smallest self-contained byte group, 1/N channels) */
int get_vgmstream_frame_size(VGMSTREAM * vgmstream);
/* In NDS IMA the frame size is the block size, so the last one is short */
int get_vgmstream_samples_per_shortframe(VGMSTREAM * vgmstream);
int get_vgmstream_shortframe_size(VGMSTREAM * vgmstream);

/* Decode samples into the buffer. Assume that we have written samples_written into the
 * buffer already, and we have samples_to_do consecutive samples ahead of us. */
void decode_vgmstream(VGMSTREAM * vgmstream, int samples_written, int samples_to_do, sample * buffer);

/* Calculate number of consecutive samples to do (taking into account stopping for loop start and end) */
int vgmstream_samples_to_do(int samples_this_block, int samples_per_frame, VGMSTREAM * vgmstream);

/* Detect loop start and save values, or detect loop end and restore (loop back). Returns 1 if loop was done. */
int vgmstream_do_loop(VGMSTREAM * vgmstream);

/* Open the stream for reading at offset (standarized taking into account layouts, channels and so on).
 * returns 0 on failure */
int vgmstream_open_stream(VGMSTREAM * vgmstream, STREAMFILE *streamFile, off_t start_offset);

/* get description info */
const char * get_vgmstream_coding_description(coding_t coding_type);
const char * get_vgmstream_layout_description(layout_t layout_type);
const char * get_vgmstream_meta_description(meta_t meta_type);

#endif
