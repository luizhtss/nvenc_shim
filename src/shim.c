#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nvenc_types.h"


static HMODULE  g_realDll             = NULL;
static PFN_NvEncodeAPICreateInstance       g_realCreateInstance = NULL;
static PFN_NvEncodeAPIGetMaxSupportedVersion g_realGetMaxVer    = NULL;

static uint32_t g_apiMajor            = 0;
static uint32_t g_apiMinor            = 0;

/* Pre-computed full version values (including bit-31 flag where needed).
 * Set during init based on detected driver API version.                  */
static uint32_t g_initParamsVer       = 0;
static uint32_t g_configVer           = 0;
static uint32_t g_presetConfigVer     = 0;
static uint32_t g_reconfigVer         = 0;

static void*    g_realFuncs[FUNC_TABLE_SLOT_COUNT];

static INIT_ONCE g_initOnce = INIT_ONCE_STATIC_INIT;

#ifdef SHIM_DEBUG
static FILE*     g_log      = NULL;
static WCHAR     g_logPath[MAX_PATH];
static CRITICAL_SECTION g_logCS;
#endif


#ifdef SHIM_DEBUG

static void log_open(HMODULE hSelf) {
    WCHAR dir[MAX_PATH];
    GetModuleFileNameW(hSelf, dir, MAX_PATH);
    WCHAR* slash = wcsrchr(dir, L'\\');
    if (slash) *(slash + 1) = L'\0';
    _snwprintf(g_logPath, MAX_PATH, L"%snvenc_shim.log", dir);
    g_log = _wfopen(g_logPath, L"a");
}

static void shim_log(const char* fmt, ...) {
    if (!g_log) return;
    EnterCriticalSection(&g_logCS);
    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(g_log, "[%02d:%02d:%02d.%03d] ",
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fprintf(g_log, "\n");
    fflush(g_log);
    LeaveCriticalSection(&g_logCS);
}

#else

#define log_open(hSelf)      ((void)0)
#define shim_log(fmt, ...)  ((void)0)

#endif

#ifdef SHIM_DEBUG

static const char* status_str(NVENCSTATUS s) {
    switch (s) {
    case NV_ENC_SUCCESS:             return "SUCCESS";
    case NV_ENC_ERR_INVALID_VERSION: return "INVALID_VERSION";
    case NV_ENC_ERR_INVALID_PARAM:   return "INVALID_PARAM";
    case NV_ENC_ERR_INVALID_CALL:    return "INVALID_CALL";
    case NV_ENC_ERR_UNSUPPORTED_PARAM: return "UNSUPPORTED_PARAM";
    case NV_ENC_ERR_GENERIC:         return "GENERIC";
    default: return "OTHER";
    }
}

static void log_guid(const char* label, const GUID* g) {
    shim_log("  %s = {%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
        label,
        (unsigned)g->Data1, g->Data2, g->Data3,
        g->Data4[0], g->Data4[1], g->Data4[2], g->Data4[3],
        g->Data4[4], g->Data4[5], g->Data4[6], g->Data4[7]);
}

#else

#define status_str(s)       ""
#define log_guid(label, g)  ((void)0)

#endif


static int guid_eq(const GUID* a, const GUID* b) {
    return memcmp(a, b, sizeof(GUID)) == 0;
}


typedef struct {
    const GUID* oldPreset;
    const GUID* newPreset;
    uint32_t    tuningInfo;
    const char* name;
} PresetMapping;

static const PresetMapping g_presetMap[] = {
    { &NV_ENC_PRESET_LOW_LATENCY_HP_GUID,      &NV_ENC_PRESET_P2_GUID, NV_ENC_TUNING_INFO_LOW_LATENCY,  "LowLatencyHP  -> P2 LowLatency"  },
    { &NV_ENC_PRESET_LOW_LATENCY_DEFAULT_GUID,  &NV_ENC_PRESET_P4_GUID, NV_ENC_TUNING_INFO_LOW_LATENCY,  "LowLatencyDef -> P4 LowLatency"  },
    { &NV_ENC_PRESET_LOW_LATENCY_HQ_GUID,       &NV_ENC_PRESET_P4_GUID, NV_ENC_TUNING_INFO_LOW_LATENCY,  "LowLatencyHQ  -> P4 LowLatency"  },
    { &NV_ENC_PRESET_DEFAULT_GUID,               &NV_ENC_PRESET_P4_GUID, NV_ENC_TUNING_INFO_HIGH_QUALITY, "Default       -> P4 HighQuality" },
    { &NV_ENC_PRESET_HP_GUID,                    &NV_ENC_PRESET_P1_GUID, NV_ENC_TUNING_INFO_HIGH_QUALITY, "HP            -> P1 HighQuality" },
    { &NV_ENC_PRESET_HQ_GUID,                    &NV_ENC_PRESET_P6_GUID, NV_ENC_TUNING_INFO_HIGH_QUALITY, "HQ            -> P6 HighQuality" },
    { &NV_ENC_PRESET_BD_GUID,                    &NV_ENC_PRESET_P5_GUID, NV_ENC_TUNING_INFO_HIGH_QUALITY, "BD            -> P5 HighQuality" },
    { &NV_ENC_PRESET_LOSSLESS_DEFAULT_GUID,      &NV_ENC_PRESET_P5_GUID, NV_ENC_TUNING_INFO_LOSSLESS,     "LosslessDef   -> P5 Lossless"    },
    { &NV_ENC_PRESET_LOSSLESS_HP_GUID,           &NV_ENC_PRESET_P3_GUID, NV_ENC_TUNING_INFO_LOSSLESS,     "LosslessHP    -> P3 Lossless"    },
};

static int is_new_preset(const GUID* g) {
    return guid_eq(g, &NV_ENC_PRESET_P1_GUID) ||
           guid_eq(g, &NV_ENC_PRESET_P2_GUID) ||
           guid_eq(g, &NV_ENC_PRESET_P3_GUID) ||
           guid_eq(g, &NV_ENC_PRESET_P4_GUID) ||
           guid_eq(g, &NV_ENC_PRESET_P5_GUID) ||
           guid_eq(g, &NV_ENC_PRESET_P6_GUID) ||
           guid_eq(g, &NV_ENC_PRESET_P7_GUID);
}

static int translate_preset(GUID* presetGUID, uint32_t* tuningInfo) {
    /* 1. Exact match against known deprecated presets */
    for (int i = 0; i < (int)(sizeof(g_presetMap) / sizeof(g_presetMap[0])); i++) {
        if (guid_eq(presetGUID, g_presetMap[i].oldPreset)) {
            shim_log("  Preset translate: %s", g_presetMap[i].name);
            *presetGUID = *g_presetMap[i].newPreset;
            *tuningInfo = g_presetMap[i].tuningInfo;
            return 1;
        }
    }

    /* 2. Already a new P1-P7 preset — pass through */
    if (is_new_preset(presetGUID)) {
        shim_log("  Preset: already new (P1-P7), no translation needed");
        return 0;
    }

    /* 3. Unknown preset (likely old/custom) — fall back to P4 + Low Latency */
    shim_log("  Preset: UNRECOGNIZED, falling back to P4 + LowLatency");
    log_guid("  original GUID", presetGUID);
    *presetGUID = NV_ENC_PRESET_P4_GUID;
    *tuningInfo = NV_ENC_TUNING_INFO_LOW_LATENCY;
    return 1;
}


static void translate_rc_mode(NV_ENC_RC_PARAMS* rc) {
    uint32_t oldMode = rc->rateControlMode;

    switch (oldMode) {
    case NV_ENC_PARAMS_RC_VBR_MINQP:
        shim_log("  RC translate: VBR_MINQP -> VBR");
        rc->rateControlMode = NV_ENC_PARAMS_RC_VBR;
        break;
    case NV_ENC_PARAMS_RC_CBR_LOWDELAY_HQ:
        shim_log("  RC translate: CBR_LOWDELAY_HQ -> CBR + QuarterRes multipass");
        rc->rateControlMode = NV_ENC_PARAMS_RC_CBR;
        rc->multiPass = NV_ENC_TWO_PASS_QUARTER_RESOLUTION;
        break;
    case NV_ENC_PARAMS_RC_CBR_HQ:
        shim_log("  RC translate: CBR_HQ -> CBR + QuarterRes multipass");
        rc->rateControlMode = NV_ENC_PARAMS_RC_CBR;
        rc->multiPass = NV_ENC_TWO_PASS_QUARTER_RESOLUTION;
        break;
    case NV_ENC_PARAMS_RC_VBR_HQ:
        shim_log("  RC translate: VBR_HQ -> VBR + QuarterRes multipass");
        rc->rateControlMode = NV_ENC_PARAMS_RC_VBR;
        rc->multiPass = NV_ENC_TWO_PASS_QUARTER_RESOLUTION;
        break;
    default:
        break;
    }
}


static uint32_t make_ver(uint32_t structVer) {
    return NVENC_MAKE_VER(g_apiMajor, g_apiMinor, structVer);
}

#define VER_FLAG_HI (1u << 31)

#ifdef SHIM_DEBUG
static void log_version(const char* label, uint32_t v) {
    shim_log("  %s = 0x%08X  (API %u.%u, struct %u)",
        label, v, NVENC_VER_MAJOR(v), NVENC_VER_MINOR(v), NVENC_VER_STRUCT(v));
}
#else
#define log_version(label, v) ((void)0)
#endif

/*
 * Generic version patcher: keeps the struct version and bit-31 flag,
 * replaces only the API major/minor with the driver's version.
 */
static uint32_t patch_ver(uint32_t v) {
    uint32_t structVer = NVENC_VER_STRUCT(v);
    uint32_t hiFlag    = v & VER_FLAG_HI;
    return make_ver(structVer) | hiFlag;
}


static void translate_init_params(NV_ENC_INITIALIZE_PARAMS* p) {
    log_version("initParams.version (old)", p->version);
    p->version = g_initParamsVer;
    log_version("initParams.version (new)", p->version);

    log_guid("initParams.encodeGUID (codec)", &p->encodeGUID);

    uint32_t tuning = p->tuningInfo;
    if (translate_preset(&p->presetGUID, &tuning))
        p->tuningInfo = tuning;

    log_guid("initParams.presetGUID", &p->presetGUID);
    shim_log("  initParams.tuningInfo     = %u", p->tuningInfo);
    shim_log("  initParams.resolution     = %ux%u", p->encodeWidth, p->encodeHeight);
    shim_log("  initParams.dar            = %ux%u", p->darWidth, p->darHeight);
    shim_log("  initParams.maxEncode      = %ux%u", p->maxEncodeWidth, p->maxEncodeHeight);
    shim_log("  initParams.frameRate      = %u/%u", p->frameRateNum, p->frameRateDen);
    shim_log("  initParams.enableAsync    = %u", p->enableEncodeAsync);
    shim_log("  initParams.enablePTD      = %u", p->enablePTD);
    shim_log("  initParams.encodeConfig   = %p", (void*)p->encodeConfig);

    NV_ENC_CONFIG* cfg = p->encodeConfig;
    if (cfg) {
        log_version("config.version (old)", cfg->version);
        cfg->version = g_configVer;
        log_version("config.version (new)", cfg->version);
        log_guid("config.profileGUID", &cfg->profileGUID);
        shim_log("  config.gopLength       = %u", cfg->gopLength);
        shim_log("  config.frameIntervalP  = %d", cfg->frameIntervalP);
        shim_log("  config.rcMode (old)    = 0x%X", cfg->rcParams.rateControlMode);
        translate_rc_mode(&cfg->rcParams);
        shim_log("  config.rcMode (new)    = 0x%X, multiPass = %u",
                  cfg->rcParams.rateControlMode, cfg->rcParams.multiPass);
        shim_log("  config.avgBitRate      = %u", cfg->rcParams.averageBitRate);
        shim_log("  config.maxBitRate      = %u", cfg->rcParams.maxBitRate);
        shim_log("  config.vbvBufSize      = %u", cfg->rcParams.vbvBufferSize);
        shim_log("  config.vbvInitDelay    = %u", cfg->rcParams.vbvInitialDelay);
    }
}


static NVENCSTATUS NVENCAPI wrap_nvEncOpenEncodeSessionEx(
    void* openParams, void** encoder)
{
    shim_log(">> nvEncOpenEncodeSessionEx");

    NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS* p =
        (NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS*)openParams;

    log_version("session.version (old)", p->version);
    p->version = make_ver(OPEN_SESSION_STRUCT_VER);
    log_version("session.version (new)", p->version);

    shim_log("  session.apiVersion (old) = 0x%08X  (API %u.%u)",
        p->apiVersion,
        p->apiVersion & 0xFFFF,
        (p->apiVersion >> 24) & 0xFF);
    p->apiVersion = g_apiMajor | (g_apiMinor << 24);
    shim_log("  session.apiVersion (new) = 0x%08X", p->apiVersion);

    PFN_nvEncOpenEncodeSessionEx realFn =
        (PFN_nvEncOpenEncodeSessionEx)g_realFuncs[FN_IDX_OPEN_SESSION_EX];
    NVENCSTATUS st = realFn(openParams, encoder);

    shim_log("<< nvEncOpenEncodeSessionEx = %d (%s)", st, status_str(st));
    return st;
}


#ifdef SHIM_DEBUG
static void log_last_error(void* encoder) {
    typedef const char* (NVENCAPI *PFN_GetErr)(void*);
    PFN_GetErr fn = (PFN_GetErr)g_realFuncs[FN_IDX_GET_LAST_ERROR];
    if (fn) {
        const char* msg = fn(encoder);
        if (msg && msg[0])
            shim_log("  lastError: %s", msg);
    }
}
#else
#define log_last_error(enc) ((void)0)
#endif

static NVENCSTATUS NVENCAPI wrap_nvEncGetEncodePresetConfig(
    void* encoder, GUID encodeGUID, GUID presetGUID, void* presetConfig)
{
    shim_log(">> nvEncGetEncodePresetConfig");
    log_guid("encodeGUID", &encodeGUID);
    log_guid("presetGUID (old)", &presetGUID);

    uint32_t tuning = NV_ENC_TUNING_INFO_UNDEFINED;
    translate_preset(&presetGUID, &tuning);
    log_guid("presetGUID (new)", &presetGUID);

    /*
     * Allocate a fully-zeroed scratch buffer for the real call.
     * SDK 13 NV_ENC_PRESET_CONFIG has several [in] reserved fields that
     * "must be set to 0".  Parsec's old-SDK buffer may have garbage there.
     */
    enum { CLEAN_SZ = 0x10000 };
    uint8_t* clean = (uint8_t*)calloc(1, CLEAN_SZ);
    if (!clean) {
        shim_log("  ERROR: calloc failed");
        return NV_ENC_ERR_OUT_OF_MEMORY;
    }

    *(uint32_t*)(clean + 0) = g_presetConfigVer;
    *(uint32_t*)(clean + 8) = g_configVer;

    log_version("presetConfig.version", *(uint32_t*)(clean + 0));
    log_version("presetCfg.version",    *(uint32_t*)(clean + 8));

    NVENCSTATUS st;
    PFN_nvEncGetEncodePresetConfigEx realEx =
        (PFN_nvEncGetEncodePresetConfigEx)g_realFuncs[FN_IDX_GET_PRESET_CONFIG_EX];

    if (realEx && tuning != NV_ENC_TUNING_INFO_UNDEFINED) {
        shim_log("  attempt 1: nvEncGetEncodePresetConfigEx (tuning=%u)", tuning);
        st = realEx(encoder, encodeGUID, presetGUID, tuning, clean);
        shim_log("  attempt 1 = %d (%s)", st, status_str(st));
    } else {
        st = NV_ENC_ERR_UNSUPPORTED_PARAM;
    }

    if (st != NV_ENC_SUCCESS) {
        log_last_error(encoder);

        shim_log("  attempt 2: nvEncGetEncodePresetConfig (non-Ex)");
        memset(clean, 0, CLEAN_SZ);
        *(uint32_t*)(clean + 0) = g_presetConfigVer;
        *(uint32_t*)(clean + 8) = g_configVer;

        PFN_nvEncGetEncodePresetConfig realFn =
            (PFN_nvEncGetEncodePresetConfig)g_realFuncs[FN_IDX_GET_PRESET_CONFIG];
        st = realFn(encoder, encodeGUID, presetGUID, clean);
        shim_log("  attempt 2 = %d (%s)", st, status_str(st));
        if (st != NV_ENC_SUCCESS)
            log_last_error(encoder);
    }

    if (st == NV_ENC_SUCCESS) {
        /*
         * Copy the NV_ENC_CONFIG result from our clean buffer back into
         * parsec's buffer.  NV_ENC_CONFIG sits at offset 8 in both old
         * and new NV_ENC_PRESET_CONFIG layouts.  4 KB covers the full
         * NV_ENC_CONFIG struct (actual size ~3 KB).
         */
        memcpy((uint8_t*)presetConfig + 8, clean + 8, 4096);
        *(uint32_t*)presetConfig = *(uint32_t*)(clean + 0);
    }

    free(clean);
    shim_log("<< nvEncGetEncodePresetConfig = %d (%s)", st, status_str(st));
    return st;
}


static NVENCSTATUS NVENCAPI wrap_nvEncInitializeEncoder(
    void* encoder, void* initParams)
{
    shim_log(">> nvEncInitializeEncoder");

    /*
     * Copy the first 136 bytes (all named fields through
     * maxMEHintCountsPerBlock[2]) into a fully zeroed buffer.
     * NVENC_EXTERNAL_ME_HINT_COUNTS_PER_BLOCKTYPE is 16 bytes each,
     * so maxMEHintCountsPerBlock[2] spans offsets 104-135.
     * Everything from offset 136 onward is reserved/new and must be zero
     * (except tuningInfo at offset 136 which translate_init_params sets).
     */
    enum { IP_BUF = 0x10000 };
    uint8_t* clean = (uint8_t*)calloc(1, IP_BUF);
    if (!clean) {
        shim_log("  ERROR: calloc failed");
        return NV_ENC_ERR_OUT_OF_MEMORY;
    }

    memcpy(clean, initParams, 136);

    translate_init_params((NV_ENC_INITIALIZE_PARAMS*)clean);

    PFN_nvEncInitializeEncoder realFn =
        (PFN_nvEncInitializeEncoder)g_realFuncs[FN_IDX_INIT_ENCODER];
    NVENCSTATUS st = realFn(encoder, clean);

    shim_log("<< nvEncInitializeEncoder = %d (%s)", st, status_str(st));
    if (st != NV_ENC_SUCCESS)
        log_last_error(encoder);

    free(clean);
    return st;
}


static NVENCSTATUS NVENCAPI wrap_nvEncReconfigureEncoder(
    void* encoder, void* reconfParams)
{
    shim_log(">> nvEncReconfigureEncoder");

    /*
     * NV_ENC_RECONFIGURE_PARAMS layout:
     *   offset 0:  uint32_t version
     *   offset 4:  uint32_t reserved          (must be 0, SDK 13)
     *   offset 8:  NV_ENC_INITIALIZE_PARAMS    (embedded, ~1800 bytes)
     *
     * Build a clean copy so all reserved/new fields are zeroed.
     */
    enum { RC_BUF = 0x10000 };
    uint8_t* clean = (uint8_t*)calloc(1, RC_BUF);
    if (!clean) {
        shim_log("  ERROR: calloc failed");
        return NV_ENC_ERR_OUT_OF_MEMORY;
    }

    log_version("reconfParams.version (old)", *(uint32_t*)reconfParams);

    *(uint32_t*)(clean + 0) = g_reconfigVer;
    log_version("reconfParams.version (new)", *(uint32_t*)(clean + 0));

    memcpy(clean + 8, (uint8_t*)reconfParams + 8, 136);

    translate_init_params((NV_ENC_INITIALIZE_PARAMS*)(clean + 8));

    PFN_nvEncReconfigureEncoder realFn =
        (PFN_nvEncReconfigureEncoder)g_realFuncs[FN_IDX_RECONFIGURE];
    NVENCSTATUS st = realFn(encoder, clean);

    shim_log("<< nvEncReconfigureEncoder = %d (%s)", st, status_str(st));
    if (st != NV_ENC_SUCCESS)
        log_last_error(encoder);

    free(clean);
    return st;
}


static BOOL CALLBACK init_once_cb(PINIT_ONCE once, PVOID param, PVOID* ctx) {
    (void)once; (void)param; (void)ctx;

    /* Load the real NVENC DLL from the system directory to avoid recursion */
    WCHAR sysDir[MAX_PATH];
    GetSystemDirectoryW(sysDir, MAX_PATH);
    WCHAR realPath[MAX_PATH];
    _snwprintf(realPath, MAX_PATH, L"%s\\nvEncodeAPI64.dll", sysDir);

    shim_log("Loading real DLL: %ls", realPath);
    g_realDll = LoadLibraryW(realPath);
    if (!g_realDll) {
        shim_log("ERROR: LoadLibrary failed (0x%08X)", (unsigned)GetLastError());
        return FALSE;
    }

    g_realGetMaxVer = (PFN_NvEncodeAPIGetMaxSupportedVersion)
        GetProcAddress(g_realDll, "NvEncodeAPIGetMaxSupportedVersion");
    g_realCreateInstance = (PFN_NvEncodeAPICreateInstance)
        GetProcAddress(g_realDll, "NvEncodeAPICreateInstance");

    if (!g_realGetMaxVer || !g_realCreateInstance) {
        shim_log("ERROR: GetProcAddress failed");
        return FALSE;
    }

    /* Discover current API version */
    uint32_t maxVer = 0;
    NVENCSTATUS st = g_realGetMaxVer(&maxVer);
    if (st != NV_ENC_SUCCESS) {
        shim_log("ERROR: GetMaxSupportedVersion returned %d", st);
        return FALSE;
    }

    /*
     * NvEncodeAPIGetMaxSupportedVersion returns (major << 4) | minor.
     * This is NOT the same as NVENCAPI_VERSION (major | minor << 24).
     */
    g_apiMajor = (maxVer >> 4) & 0xFFF;
    g_apiMinor = maxVer & 0xF;
    shim_log("Driver NVENC API: %u.%u  (raw 0x%08X, NVENCAPI_VERSION=0x%08X)",
              g_apiMajor, g_apiMinor, maxVer,
              g_apiMajor | (g_apiMinor << 24));

    /*
     * Compute full version values for each struct type.
     * Struct version numbers and the bit-31 flag differ between SDK generations.
     * Source: nvEncodeAPI.h from each SDK release.
     */
    if (g_apiMajor >= 13) {
        g_initParamsVer   = make_ver(7) | VER_FLAG_HI;
        g_configVer       = make_ver(9) | VER_FLAG_HI;
        g_presetConfigVer = make_ver(5) | VER_FLAG_HI;
        g_reconfigVer     = make_ver(2) | VER_FLAG_HI;
    } else if (g_apiMajor >= 12) {
        g_initParamsVer   = make_ver(6);
        g_configVer       = make_ver(8) | VER_FLAG_HI;
        g_presetConfigVer = make_ver(4) | VER_FLAG_HI;
        g_reconfigVer     = make_ver(1);
    } else if (g_apiMajor >= 10) {
        g_initParamsVer   = make_ver(6);
        g_configVer       = make_ver(8) | VER_FLAG_HI;
        g_presetConfigVer = make_ver(4) | VER_FLAG_HI;
        g_reconfigVer     = make_ver(1);
    } else {
        g_initParamsVer   = make_ver(5);
        g_configVer       = make_ver(7) | VER_FLAG_HI;
        g_presetConfigVer = make_ver(4) | VER_FLAG_HI;
        g_reconfigVer     = make_ver(1);
    }

    shim_log("Version table: initParams=0x%08X config=0x%08X presetCfg=0x%08X reconf=0x%08X",
              g_initParamsVer, g_configVer, g_presetConfigVer, g_reconfigVer);

    return TRUE;
}


typedef NVENCSTATUS (NVENCAPI *PFN_enc_op)(void*, void*);

#define GENERIC_WRAP(func, idx)                                         \
static NVENCSTATUS NVENCAPI wrap_##func(void* enc, void* p) {           \
    uint32_t* ver = (uint32_t*)p;                                       \
    uint32_t old = *ver;                                                \
    *ver = patch_ver(old);                                              \
    shim_log(">> " #func " ver 0x%08X->0x%08X", old, *ver);           \
    NVENCSTATUS st = ((PFN_enc_op)g_realFuncs[idx])(enc, p);           \
    shim_log("<< " #func " = %d (%s)", st, status_str(st));           \
    if (st != NV_ENC_SUCCESS)                                           \
        log_last_error(enc);                                            \
    return st;                                                          \
}

GENERIC_WRAP(CreateInputBuffer,       FN_IDX_CREATE_INPUT_BUF)
GENERIC_WRAP(CreateBitstreamBuffer,   FN_IDX_CREATE_BITSTREAM_BUF)
GENERIC_WRAP(LockBitstream,           FN_IDX_LOCK_BITSTREAM)
GENERIC_WRAP(LockInputBuffer,         FN_IDX_LOCK_INPUT_BUF)
GENERIC_WRAP(GetEncodeStats,          FN_IDX_GET_ENCODE_STATS)
GENERIC_WRAP(GetSequenceParams,       FN_IDX_GET_SEQ_PARAMS)
GENERIC_WRAP(RegisterAsyncEvent,      FN_IDX_REG_ASYNC_EVENT)
GENERIC_WRAP(UnregisterAsyncEvent,    FN_IDX_UNREG_ASYNC_EVENT)
/* ── GetEncodeCaps — has 4 params: (encoder, GUID, NV_ENC_CAPS_PARAM*, int*) ──
 * NV_ENC_CAPS_PARAM: offset 0 = version, offset 4 = capsToQuery
 * Cannot use GENERIC_WRAP because GUID is 2nd param (not a versioned struct).
 */
typedef NVENCSTATUS (NVENCAPI *PFN_nvEncGetEncodeCaps)(
    void* encoder, GUID encodeGUID, void* capsParam, int* capsVal);

static NVENCSTATUS NVENCAPI wrap_GetEncodeCaps(
    void* enc, GUID encodeGUID, void* capsParam, int* capsVal)
{
    uint32_t* ver = (uint32_t*)capsParam;
    uint32_t old = *ver;
    *ver = patch_ver(old);

#ifdef SHIM_DEBUG
    uint32_t capsQuery = *(uint32_t*)((uint8_t*)capsParam + 4);
    log_guid("GetEncodeCaps encodeGUID", &encodeGUID);
    shim_log(">> GetEncodeCaps ver 0x%08X->0x%08X capsToQuery=%u",
              old, *ver, capsQuery);
#endif

    NVENCSTATUS st = ((PFN_nvEncGetEncodeCaps)g_realFuncs[FN_IDX_GET_ENCODE_CAPS])(
        enc, encodeGUID, capsParam, capsVal);

    shim_log("<< GetEncodeCaps = %d (%s) val=%d", st, status_str(st),
              capsVal ? *capsVal : -1);
    if (st != NV_ENC_SUCCESS) log_last_error(enc);
    return st;
}

/* ── RegisterResource — log resource type and dimensions ── */
static NVENCSTATUS NVENCAPI wrap_RegisterResource(void* enc, void* p) {
    uint32_t* w = (uint32_t*)p;
    uint32_t old = w[0];
    w[0] = patch_ver(old);
#ifdef SHIM_DEBUG
    shim_log(">> RegisterResource ver 0x%08X->0x%08X type=%u %ux%u",
              old, w[0], w[1], w[2], w[3]);
#endif
    NVENCSTATUS st = ((PFN_enc_op)g_realFuncs[FN_IDX_REG_RESOURCE])(enc, p);
    shim_log("<< RegisterResource = %d (%s)", st, status_str(st));
    if (st != NV_ENC_SUCCESS) log_last_error(enc);
    return st;
}

/* ── MapInputResource — log handles ── */
static NVENCSTATUS NVENCAPI wrap_MapInputResource(void* enc, void* p) {
    uint32_t* w = (uint32_t*)p;
    uint32_t old = w[0];
    w[0] = patch_ver(old);
    shim_log(">> MapInputResource ver 0x%08X->0x%08X", old, w[0]);
    NVENCSTATUS st = ((PFN_enc_op)g_realFuncs[FN_IDX_MAP_INPUT_RESOURCE])(enc, p);
    shim_log("<< MapInputResource = %d (%s)", st, status_str(st));
    if (st != NV_ENC_SUCCESS) log_last_error(enc);
    return st;
}

/* ── EncodePicture — log input/output + pic type ──
 * NV_ENC_PIC_PARAMS layout (x64):
 *   0: version (4)   4: inputWidth (4)   8: inputHeight (4)
 *  12: inputPitch (4)  16: encodePicFlags (4)  20: frameIdx (4)
 *  24: inputTimeStamp (8)  32: inputDuration (8)
 *  40: inputBuffer (8, ptr)  48: outputBitstream (8, ptr)
 *  56: completionEvent (8, ptr)
 *  64: bufferFmt (4)  68: pictureStruct (4)  72: pictureType (4)
 */
static NVENCSTATUS NVENCAPI wrap_EncodePicture(void* enc, void* p) {
    uint8_t* b = (uint8_t*)p;
    uint32_t old = *(uint32_t*)(b + 0);
    *(uint32_t*)(b + 0) = patch_ver(old);

#ifdef SHIM_DEBUG
    shim_log(">> EncodePicture ver 0x%08X->0x%08X", old, *(uint32_t*)(b + 0));
    shim_log("   %ux%u pitch=%u flags=0x%X frame=%u fmt=%u struct=%u type=%u",
              *(uint32_t*)(b+4), *(uint32_t*)(b+8), *(uint32_t*)(b+12),
              *(uint32_t*)(b+16), *(uint32_t*)(b+20),
              *(uint32_t*)(b+64), *(uint32_t*)(b+68), *(uint32_t*)(b+72));
    shim_log("   inputBuf=%p outputBuf=%p", *(void**)(b+40), *(void**)(b+48));
#endif

    NVENCSTATUS st = ((PFN_enc_op)g_realFuncs[FN_IDX_ENCODE_PICTURE])(enc, p);
    shim_log("<< EncodePicture = %d (%s)", st, status_str(st));
    if (st != NV_ENC_SUCCESS && st != 17)
        log_last_error(enc);
    return st;
}

static BOOL ensure_init(void) {
    return InitOnceExecuteOnce(&g_initOnce, init_once_cb, NULL, NULL);
}


__declspec(dllexport)
NVENCSTATUS NVENCAPI NvEncodeAPIGetMaxSupportedVersion(uint32_t* version) {
    if (!ensure_init()) return NV_ENC_ERR_GENERIC;
    shim_log(">> NvEncodeAPIGetMaxSupportedVersion");
    NVENCSTATUS st = g_realGetMaxVer(version);
    shim_log("<< NvEncodeAPIGetMaxSupportedVersion = %d  (ver 0x%08X)", st,
              version ? *version : 0);
    return st;
}


__declspec(dllexport)
NVENCSTATUS NVENCAPI NvEncodeAPICreateInstance(
    NV_ENCODE_API_FUNCTION_LIST* funcList)
{
    if (!ensure_init()) return NV_ENC_ERR_GENERIC;
    shim_log(">> NvEncodeAPICreateInstance");

    if (!funcList) {
        shim_log("   ERROR: funcList is NULL");
        return NV_ENC_ERR_INVALID_PTR;
    }

    log_version("funcList.version (caller)", funcList->version);

    /* Use the current driver's API version so the real function accepts it */
    funcList->version = make_ver(FUNC_LIST_STRUCT_VER);
    log_version("funcList.version (patched)", funcList->version);

    NVENCSTATUS st = g_realCreateInstance(funcList);
    shim_log("   real NvEncodeAPICreateInstance = %d (%s)", st, status_str(st));

    if (st != NV_ENC_SUCCESS)
        return st;

    /* Snapshot all real function pointers */
    memcpy(g_realFuncs, funcList->funcs, sizeof(g_realFuncs));

    /* Install our wrappers for the functions that need translation */
    funcList->funcs[FN_IDX_OPEN_SESSION_EX]    = (void*)wrap_nvEncOpenEncodeSessionEx;
    funcList->funcs[FN_IDX_GET_PRESET_CONFIG]  = (void*)wrap_nvEncGetEncodePresetConfig;
    funcList->funcs[FN_IDX_INIT_ENCODER]       = (void*)wrap_nvEncInitializeEncoder;
    funcList->funcs[FN_IDX_RECONFIGURE]        = (void*)wrap_nvEncReconfigureEncoder;

    funcList->funcs[FN_IDX_CREATE_INPUT_BUF]   = (void*)wrap_CreateInputBuffer;
    funcList->funcs[FN_IDX_CREATE_BITSTREAM_BUF]= (void*)wrap_CreateBitstreamBuffer;
    funcList->funcs[FN_IDX_ENCODE_PICTURE]     = (void*)wrap_EncodePicture;
    funcList->funcs[FN_IDX_LOCK_BITSTREAM]     = (void*)wrap_LockBitstream;
    funcList->funcs[FN_IDX_LOCK_INPUT_BUF]     = (void*)wrap_LockInputBuffer;
    funcList->funcs[FN_IDX_GET_ENCODE_STATS]   = (void*)wrap_GetEncodeStats;
    funcList->funcs[FN_IDX_GET_SEQ_PARAMS]     = (void*)wrap_GetSequenceParams;
    funcList->funcs[FN_IDX_REG_ASYNC_EVENT]    = (void*)wrap_RegisterAsyncEvent;
    funcList->funcs[FN_IDX_UNREG_ASYNC_EVENT]  = (void*)wrap_UnregisterAsyncEvent;
    funcList->funcs[FN_IDX_MAP_INPUT_RESOURCE] = (void*)wrap_MapInputResource;
    funcList->funcs[FN_IDX_REG_RESOURCE]       = (void*)wrap_RegisterResource;
    funcList->funcs[FN_IDX_GET_ENCODE_CAPS]    = (void*)wrap_GetEncodeCaps;

    shim_log("   Hooked 16 functions (4 specialized + 12 generic version patch)");
    shim_log("<< NvEncodeAPICreateInstance = SUCCESS");
    return NV_ENC_SUCCESS;
}


BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, LPVOID reserved) {
    (void)reserved;
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hInst);
#ifdef SHIM_DEBUG
        InitializeCriticalSection(&g_logCS);
        log_open(hInst);
        shim_log("=== nvenc_shim loaded (pid %u) ===", (unsigned)GetCurrentProcessId());
#else
        (void)hInst;
#endif
        break;
    case DLL_PROCESS_DETACH:
        shim_log("=== nvenc_shim unloading ===");
        if (g_realDll) FreeLibrary(g_realDll);
#ifdef SHIM_DEBUG
        if (g_log) fclose(g_log);
        DeleteCriticalSection(&g_logCS);
#endif
        break;
    }
    return TRUE;
}
