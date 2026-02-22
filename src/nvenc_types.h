#ifndef NVENC_TYPES_H
#define NVENC_TYPES_H

#include <windows.h>
#include <stdint.h>

typedef int NVENCSTATUS;

#define NV_ENC_SUCCESS                     0
#define NV_ENC_ERR_NO_ENCODE_DEVICE        1
#define NV_ENC_ERR_UNSUPPORTED_DEVICE      2
#define NV_ENC_ERR_INVALID_ENCODERDEVICE   3
#define NV_ENC_ERR_INVALID_DEVICE          4
#define NV_ENC_ERR_DEVICE_NOT_EXIST        5
#define NV_ENC_ERR_INVALID_PTR             6
#define NV_ENC_ERR_INVALID_EVENT           7
#define NV_ENC_ERR_INVALID_PARAM           8
#define NV_ENC_ERR_INVALID_CALL            9
#define NV_ENC_ERR_OUT_OF_MEMORY          10
#define NV_ENC_ERR_ENCODER_NOT_INITIALIZED 11
#define NV_ENC_ERR_UNSUPPORTED_PARAM      12
#define NV_ENC_ERR_LOCK_BUSY              13
#define NV_ENC_ERR_NOT_ENOUGH_BUFFER      14
#define NV_ENC_ERR_INVALID_VERSION        15
#define NV_ENC_ERR_MAP_FAILED             16
#define NV_ENC_ERR_NEED_MORE_INPUT        17
#define NV_ENC_ERR_ENCODER_BUSY           18
#define NV_ENC_ERR_EVENT_NOT_REGISTERD    19
#define NV_ENC_ERR_GENERIC                20

/*
 * NVENC version encoding (uint32_t):
 *   Bits  0-15: API major version
 *   Bits 16-23: Struct version
 *   Bits 24-27: API minor version
 *   Bits 28-31: 0x7 (magic tag)
 */
#define NVENC_VER_MAJOR(v)  ((v) & 0xFFFF)
#define NVENC_VER_STRUCT(v) (((v) >> 16) & 0xFF)
#define NVENC_VER_MINOR(v)  (((v) >> 24) & 0xF)
#define NVENC_MAKE_VER(major, minor, sver) \
    ((uint32_t)(0x7u << 28) | ((uint32_t)(minor) << 24) | ((uint32_t)(sver) << 16) | (uint32_t)(major))

#define NVENCAPI __stdcall

/* ── Struct versions that are stable across all SDK versions ── */
#define FUNC_LIST_STRUCT_VER        2
#define OPEN_SESSION_STRUCT_VER     1

/* ── Function table indices ── */
#define FN_IDX_OPEN_SESSION           0
#define FN_IDX_GET_ENCODE_GUID_COUNT  1
#define FN_IDX_GET_ENCODE_GUIDS       2
#define FN_IDX_GET_PROFILE_GUID_COUNT 3
#define FN_IDX_GET_PROFILE_GUIDS      4
#define FN_IDX_GET_INPUT_FMT_COUNT    5
#define FN_IDX_GET_INPUT_FMTS         6
#define FN_IDX_GET_ENCODE_CAPS        7
#define FN_IDX_GET_PRESET_COUNT       8
#define FN_IDX_GET_PRESET_GUIDS       9
#define FN_IDX_GET_PRESET_CONFIG     10
#define FN_IDX_INIT_ENCODER          11
#define FN_IDX_CREATE_INPUT_BUF      12
#define FN_IDX_DESTROY_INPUT_BUF     13
#define FN_IDX_CREATE_BITSTREAM_BUF  14
#define FN_IDX_DESTROY_BITSTREAM_BUF 15
#define FN_IDX_ENCODE_PICTURE        16
#define FN_IDX_LOCK_BITSTREAM        17
#define FN_IDX_UNLOCK_BITSTREAM      18
#define FN_IDX_LOCK_INPUT_BUF        19
#define FN_IDX_UNLOCK_INPUT_BUF      20
#define FN_IDX_GET_ENCODE_STATS      21
#define FN_IDX_GET_SEQ_PARAMS        22
#define FN_IDX_REG_ASYNC_EVENT       23
#define FN_IDX_UNREG_ASYNC_EVENT     24
#define FN_IDX_MAP_INPUT_RESOURCE    25
#define FN_IDX_UNMAP_INPUT_RESOURCE  26
#define FN_IDX_DESTROY_ENCODER       27
#define FN_IDX_INVALIDATE_REF_FRAMES 28
#define FN_IDX_OPEN_SESSION_EX       29
#define FN_IDX_REG_RESOURCE          30
#define FN_IDX_UNREG_RESOURCE        31
#define FN_IDX_RECONFIGURE           32
#define FN_IDX_RESERVED1             33
#define FN_IDX_CREATE_MV_BUF         34
#define FN_IDX_DESTROY_MV_BUF        35
#define FN_IDX_RUN_MOTION_EST        36
#define FN_IDX_GET_LAST_ERROR        37
#define FN_IDX_SET_IO_CUDA_STREAMS   38
#define FN_IDX_GET_PRESET_CONFIG_EX  39
#define FN_IDX_GET_SEQ_PARAMS_EX     40

#define FUNC_TABLE_SLOT_COUNT 318

/* ── NV_ENCODE_API_FUNCTION_LIST ── */
typedef struct {
    uint32_t version;
    uint32_t reserved;
    void*    funcs[FUNC_TABLE_SLOT_COUNT];
} NV_ENCODE_API_FUNCTION_LIST;

/* ── Deprecated Preset GUIDs (removed in driver R550+) ── */
static const GUID NV_ENC_PRESET_DEFAULT_GUID =
    { 0xB2DFB705, 0x4EBD, 0x4C49, { 0x9B, 0x5F, 0x24, 0xA7, 0x77, 0xD3, 0xE5, 0x87 } };
static const GUID NV_ENC_PRESET_HP_GUID =
    { 0x60E4C59F, 0xE846, 0x4484, { 0xA5, 0x6D, 0xCD, 0x45, 0xBE, 0x9F, 0xDD, 0xF6 } };
static const GUID NV_ENC_PRESET_HQ_GUID =
    { 0x34DBA71D, 0xA77B, 0x4B8F, { 0x9C, 0x3E, 0xB6, 0xD5, 0xDA, 0x24, 0xC0, 0x12 } };
static const GUID NV_ENC_PRESET_BD_GUID =
    { 0x82E3E450, 0xBDBB, 0x4E40, { 0x98, 0x9C, 0x82, 0xA9, 0x0D, 0xF9, 0xEF, 0x32 } };
static const GUID NV_ENC_PRESET_LOW_LATENCY_DEFAULT_GUID =
    { 0x49DF21C5, 0x6DFA, 0x4FEB, { 0x97, 0x87, 0x6A, 0xCC, 0x9E, 0xFF, 0xB7, 0x26 } };
static const GUID NV_ENC_PRESET_LOW_LATENCY_HQ_GUID =
    { 0xC5F733B9, 0xEA97, 0x4CF9, { 0xBE, 0xC2, 0xBF, 0x78, 0xA7, 0x4F, 0xD1, 0x05 } };
static const GUID NV_ENC_PRESET_LOW_LATENCY_HP_GUID =
    { 0x67082A44, 0x4BAD, 0x48FA, { 0x98, 0xEA, 0x93, 0x05, 0x6D, 0x15, 0x0A, 0x58 } };
static const GUID NV_ENC_PRESET_LOSSLESS_DEFAULT_GUID =
    { 0xD5BFB716, 0xC604, 0x44E7, { 0x9B, 0xB8, 0xDE, 0xA5, 0x51, 0x0F, 0xC3, 0xAC } };
static const GUID NV_ENC_PRESET_LOSSLESS_HP_GUID =
    { 0x149998E7, 0x2364, 0x411D, { 0x82, 0xEF, 0x17, 0x98, 0x88, 0x09, 0x34, 0x09 } };

/* ── New Preset GUIDs (P1-P7, SDK 10+) ── */
static const GUID NV_ENC_PRESET_P1_GUID =
    { 0xFC0A8D3E, 0x45F8, 0x4CF8, { 0x80, 0xC7, 0x29, 0x88, 0x71, 0x59, 0x0E, 0xBF } };
static const GUID NV_ENC_PRESET_P2_GUID =
    { 0xF581CFB8, 0x88D6, 0x4381, { 0x93, 0xF0, 0xDF, 0x13, 0xF9, 0xC2, 0x7D, 0xAB } };
static const GUID NV_ENC_PRESET_P3_GUID =
    { 0x36850110, 0x3A07, 0x441F, { 0x94, 0xD5, 0x36, 0x70, 0x63, 0x1F, 0x91, 0xF6 } };
static const GUID NV_ENC_PRESET_P4_GUID =
    { 0x90A7B826, 0xDF06, 0x4862, { 0xB9, 0xD2, 0xCD, 0x6D, 0x73, 0xA0, 0x86, 0x81 } };
static const GUID NV_ENC_PRESET_P5_GUID =
    { 0x21C6E6B4, 0x297A, 0x4CBA, { 0x99, 0x8F, 0xB6, 0xCB, 0xDE, 0x72, 0xAD, 0xE3 } };
static const GUID NV_ENC_PRESET_P6_GUID =
    { 0x8E75C279, 0x6299, 0x4AB6, { 0x83, 0x02, 0x0B, 0x21, 0x5A, 0x33, 0x5C, 0xF5 } };
static const GUID NV_ENC_PRESET_P7_GUID =
    { 0x84848C12, 0x6F71, 0x4C13, { 0x93, 0x1B, 0x53, 0xE2, 0x83, 0xF5, 0x79, 0x74 } };

/* ── Tuning Info (SDK 10+) ── */
#define NV_ENC_TUNING_INFO_UNDEFINED          0
#define NV_ENC_TUNING_INFO_HIGH_QUALITY       1
#define NV_ENC_TUNING_INFO_LOW_LATENCY        2
#define NV_ENC_TUNING_INFO_ULTRA_LOW_LATENCY  3
#define NV_ENC_TUNING_INFO_LOSSLESS           4

/* ── Rate Control Modes ── */
#define NV_ENC_PARAMS_RC_CONSTQP            0x0
#define NV_ENC_PARAMS_RC_VBR                0x1
#define NV_ENC_PARAMS_RC_CBR                0x2
#define NV_ENC_PARAMS_RC_VBR_MINQP          0x4   /* deprecated */
#define NV_ENC_PARAMS_RC_CBR_LOWDELAY_HQ    0x8   /* deprecated */
#define NV_ENC_PARAMS_RC_CBR_HQ             0x10  /* deprecated */
#define NV_ENC_PARAMS_RC_VBR_HQ             0x20  /* deprecated */

/* ── Multi-Pass (SDK 10+) ── */
#define NV_ENC_MULTI_PASS_DISABLED                0x0
#define NV_ENC_TWO_PASS_QUARTER_RESOLUTION        0x1
#define NV_ENC_TWO_PASS_FULL_RESOLUTION           0x2

/*
 * ── NV_ENC_RC_PARAMS ──
 * Layout matches SDK 10-12.  multiPass sits where reserved[0] was in SDK 9.
 */
typedef struct {
    uint32_t rateControlMode;          /*  0 */
    uint32_t constQP[3];               /*  4  (NV_ENC_QP: InterP, InterB, Intra) */
    uint32_t averageBitRate;           /* 16 */
    uint32_t maxBitRate;               /* 20 */
    uint32_t vbvBufferSize;            /* 24 */
    uint32_t vbvInitialDelay;          /* 28 */
    uint32_t flags;                    /* 32  (packed bitfields) */
    uint32_t minQP[3];                 /* 36 */
    uint32_t maxQP[3];                 /* 48 */
    uint32_t initialRCQP[3];          /* 60 */
    uint32_t temporallayerIdxMask;    /* 72 */
    uint8_t  temporalLayerQP[8];      /* 76 */
    uint16_t targetQuality;           /* 84 */
    uint16_t targetQualityLSB;        /* 86 */
    uint16_t lookaheadDepth;          /* 88 */
    uint16_t lowDelayKeyFrameScale;   /* 90 */
    uint32_t multiPass;               /* 92  (was reserved[0] in SDK 9) */
    uint32_t alphaLayerBitrateRatio;  /* 96 */
    uint32_t reserved[3];             /* 100 */
} NV_ENC_RC_PARAMS;

/*
 * ── NV_ENC_CONFIG (partial) ──
 * Only fields up to rcParams are defined.  We never allocate this; only access
 * through a pointer provided by the caller, so the partial size is harmless.
 */
typedef struct {
    uint32_t         version;              /*   0 */
    GUID             profileGUID;          /*   4 */
    uint32_t         gopLength;            /*  20 */
    int32_t          frameIntervalP;       /*  24 */
    uint32_t         monoChromeEncoding;   /*  28 */
    uint32_t         frameFieldMode;       /*  32 */
    uint32_t         mvPrecision;          /*  36 */
    NV_ENC_RC_PARAMS rcParams;             /*  40 */
} NV_ENC_CONFIG;

/*
 * ── NV_ENC_INITIALIZE_PARAMS ──
 * Full layout (SDK 10+ view).  tuningInfo occupies what was reserved[0] in SDK 9.
 * Total size: 1792 bytes on x64 — identical across SDK 9-12.
 */
typedef struct {
    uint32_t       version;                    /*   0 */
    GUID           encodeGUID;                 /*   4 */
    GUID           presetGUID;                 /*  20 */
    uint32_t       encodeWidth;                /*  36 */
    uint32_t       encodeHeight;               /*  40 */
    uint32_t       darWidth;                   /*  44 */
    uint32_t       darHeight;                  /*  48 */
    uint32_t       frameRateNum;               /*  52 */
    uint32_t       frameRateDen;               /*  56 */
    uint32_t       enableEncodeAsync;          /*  60 */
    uint32_t       enablePTD;                  /*  64 */
    uint32_t       bitfields;                  /*  68 */
    uint32_t       privDataSize;               /*  72 */
    uint32_t       reservedPad;                /*  76  (explicit in SDK 13, implicit padding before) */
    void*          privData;                   /*  80 */
    NV_ENC_CONFIG* encodeConfig;               /*  88 */
    uint32_t       maxEncodeWidth;             /*  96 */
    uint32_t       maxEncodeHeight;            /* 100 */
    uint32_t       maxMEHintCountsPerBlock[8]; /* 104  (2 × 16-byte NVENC_EXTERNAL_ME_HINT_COUNTS_PER_BLOCKTYPE) */
    uint32_t       tuningInfo;                 /* 136  (was reserved[0] in old SDKs) */
    uint32_t       reserved[288];              /* 140 */
    void*          reserved2[64];              /* 1292 */
} NV_ENC_INITIALIZE_PARAMS;

/*
 * ── NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS ──
 */
typedef struct {
    uint32_t version;              /*    0 */
    uint32_t deviceType;           /*    4 */
    void*    device;               /*    8 */
    void*    reserved;             /*   16 */
    uint32_t apiVersion;           /*   24  — must match driver's expected API version */
    uint32_t reserved1[253];       /*   28 */
    void*    reserved2[64];        /* 1040 */
} NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS;

/* ── Exported entry-point typedefs ── */
typedef NVENCSTATUS (NVENCAPI *PFN_NvEncodeAPICreateInstance)(NV_ENCODE_API_FUNCTION_LIST*);
typedef NVENCSTATUS (NVENCAPI *PFN_NvEncodeAPIGetMaxSupportedVersion)(uint32_t*);

/* ── Function pointer typedefs for individual NVENC functions we wrap ── */
typedef NVENCSTATUS (NVENCAPI *PFN_nvEncOpenEncodeSessionEx)(void* params, void** encoder);
typedef NVENCSTATUS (NVENCAPI *PFN_nvEncGetEncodePresetConfig)(void* encoder, GUID encGUID, GUID presetGUID, void* presetCfg);
typedef NVENCSTATUS (NVENCAPI *PFN_nvEncInitializeEncoder)(void* encoder, void* initParams);
typedef NVENCSTATUS (NVENCAPI *PFN_nvEncReconfigureEncoder)(void* encoder, void* reconfParams);
typedef NVENCSTATUS (NVENCAPI *PFN_nvEncGetEncodePresetConfigEx)(void* encoder, GUID encGUID, GUID presetGUID, uint32_t tuningInfo, void* presetCfg);

#endif /* NVENC_TYPES_H */
