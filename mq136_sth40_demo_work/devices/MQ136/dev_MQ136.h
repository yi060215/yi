#ifndef __DEV_MQ136_H
#define __DEV_MQ136_H

/* ================================================================ */
/*  Hardware Parameters                                             */
/* ================================================================ */

/* ADC reference voltage (V) */
#define MQ136_ADC_VREF          3.3f

/* ADC resolution: 12-bit = 4096 levels */
#define MQ136_ADC_MAX           4096.0f

/* RL: load resistor value in kOhm on the MQ136 module */
#define MQ136_RL_KOHM           10.0f

/* Module supply voltage (V) */
#define MQ136_VC                5.0f

/* ================================================================ */
/*  Signal Filtering                                                */
/* ================================================================ */

/* Moving average filter window size */
#define MQ136_FILTER_WINDOW     16

/* Number of samples for initial median baseline calibration */
#define MQ136_CALIB_SAMPLES     100

/* ================================================================ */
/*  Adaptive Baseline Tracking                                      */
/* ================================================================ */

/* EMA alpha for baseline update (per sample, 100ms) */
#define BASELINE_EMA_ALPHA      0.002f

/* Only update baseline when Rs/Ro >= this threshold (clean air) */
#define BASELINE_UPDATE_MIN_RSRO 0.95f

/* Warn if baseline drifts more than this fraction from initial */
#define BASELINE_DRIFT_WARN     0.20f

/* ================================================================ */
/*  Breath Event Detection State Machine                            */
/* ================================================================ */

/* Rs/Ro below this triggers a candidate breath event */
#define BED_TRIGGER_RATIO       0.90f

/* Rs/Ro above this signals recovery to baseline */
#define BED_RECOVERY_RATIO      0.95f

/* Consecutive samples to confirm event start (reject noise) */
#define BED_CONFIRM_CNT         3

/* Consecutive samples to confirm recovery (hysteresis) */
#define BED_RECOVERY_CNT        5

/* Minimum event duration (ms) — shorter = artifact */
#define BED_MIN_DURATION_MS     500

/* Maximum event duration (ms) — longer = timeout */
#define BED_MAX_DURATION_MS     30000

/* ================================================================ */
/*  Temperature & Humidity Compensation                             */
/* ================================================================ */

#define MQ136_COMP_T_REF        20.0f    /* Reference temperature (C) */
#define MQ136_COMP_RH_REF       65.0f    /* Reference humidity (%RH) */
#define MQ136_COMP_ALPHA        (-0.012f) /* Temperature coefficient (per C) */
#define MQ136_COMP_BETA         (-0.006f) /* Humidity coefficient (per %RH) */
#define MQ136_COMP_GAMMA        0.0001f  /* T*RH interaction term */

/* Clamp range for compensation factor */
#define MQ136_COMP_FACTOR_MIN   0.5f
#define MQ136_COMP_FACTOR_MAX   1.5f

/* ================================================================ */
/*  Rs/Ro -> ppb Conversion (Power Law)                             */
/* ================================================================ */

/* MQ136 sensitivity: Rs/Ro = A * ppm^B
   A = 0.85, B = -0.35  =>  ppm = (A / RsRo) ^ (1/|B|) = (A / RsRo) ^ 2.857 */
#define MQ136_SENS_A            0.85f
#define MQ136_SENS_B            (-0.35f)
#define MQ136_PPB_EXPONENT      2.857f   /* 1/|B| */

/* Minimum reportable ppb (below this = effectively zero) */
#define MQ136_PPB_FLOOR         5.0f

/* ================================================================ */
/*  ppb-Based Classification (clinical thresholds)                  */
/* ================================================================ */

#define MQ136_PPB_NORMAL        30.0f    /* < 30 ppb: healthy */
#define MQ136_PPB_MILD          100.0f   /* 30-100 ppb: mild halitosis */
#define MQ136_PPB_MODERATE      300.0f   /* 100-300 ppb: noticeable halitosis */
                                         /* > 300 ppb: severe halitosis */

/* ================================================================ */
/*  Confidence Scoring                                              */
/* ================================================================ */

/* Minimum peak depth (1 - RsRo) for full magnitude confidence */
#define CONF_PEAK_FULL_DEPTH    0.50f

/* Ideal breath event duration range (ms) */
#define CONF_DUR_IDEAL_MIN      2000
#define CONF_DUR_IDEAL_MAX      8000
#define CONF_DUR_ACCEPT_MIN     500
#define CONF_DUR_ACCEPT_MAX     15000

/* SNR threshold: peak_depth / baseline_noise >= threshold = full confidence */
#define CONF_SNR_FULL           10.0f

/* Valid operating T&H range */
#define CONF_T_MIN              15.0f
#define CONF_T_MAX              35.0f
#define CONF_RH_MIN             30.0f
#define CONF_RH_MAX             90.0f

/* Confidence weights */
#define CONF_W_MAGNITUDE        0.30f
#define CONF_W_DURATION         0.30f
#define CONF_W_SNR              0.30f
#define CONF_W_ENV              0.10f

/* ================================================================ */
/*  Enums                                                           */
/* ================================================================ */

/* Breath quality classification */
typedef enum {
    BREATH_NORMAL   = 0,
    BREATH_MILD     = 1,
    BREATH_MODERATE = 2,
    BREATH_SEVERE   = 3
} BreathLevel;

/* Breath event state machine states */
typedef enum {
    BREATH_STATE_IDLE          = 0,
    BREATH_STATE_RISING_EDGE   = 1,
    BREATH_STATE_BREATHING     = 2,
    BREATH_STATE_FALLING_EDGE  = 3
} BreathState;

/* ================================================================ */
/*  Device Structure                                                */
/* ================================================================ */

typedef struct MQ136Dev {
    /* Identification */
    char       *name;

    /* Baseline (Ro) — continuously tracked */
    float       baseline_voltage;      /* Vout in clean air (V) */
    float       baseline_rs;           /* Sensor resistance in clean air (kOhm) = Ro */
    float       initial_baseline_rs;   /* Frozen copy from initial calibration */
    int         baseline_frozen;       /* 1 = baseline update paused */

    /* Current sample */
    float       current_voltage;       /* Latest ADC voltage reading (V) */
    float       current_rs;            /* Latest sensor resistance (kOhm) */
    float       rs_ro_ratio;           /* Rs/Ro ratio (raw, per-sample) */
    float       rs_ro_compensated;     /* Rs/Ro ratio (T&H compensated, per-sample) */
    BreathLevel level;                 /* Live per-sample classification (for debug) */

    /* Moving average filter */
    float       voltage_buffer[MQ136_FILTER_WINDOW];
    int         buffer_index;

    /* --- Breath event state machine --- */
    BreathState event_state;
    int         event_confirm_cnt;     /* Consecutive trigger samples */
    int         event_sample_cnt;      /* Samples inside breathing state */
    int         event_recovery_cnt;    /* Consecutive recovery samples */

    /* --- Peak tracking during breath event --- */
    float       event_peak_rs_ro;      /* Minimum Rs/Ro observed (peak H2S) */
    float       event_peak_temperature;
    float       event_peak_humidity;

    /* --- Last completed breath event result --- */
    float       last_ppb_h2s;
    float       last_confidence;
    int         last_event_valid;      /* 1 = new result available */
    int         last_event_duration_ms;
    BreathLevel last_level;

    /* Function pointers */
    int         (*Init)(struct MQ136Dev *ptdev);
    int         (*Calibrate)(struct MQ136Dev *ptdev);
    int         (*Read)(struct MQ136Dev *ptdev);
    int         (*Process)(struct MQ136Dev *ptdev, float temperature, float humidity);
} MQ136Device;

/* ================================================================ */
/*  Public API                                                      */
/* ================================================================ */

/* Get singleton device instance */
struct MQ136Dev *MQ136GetDevice(void);

/* Core: call at 100ms intervals. Reads ADC, updates baseline, runs state machine.
   On breath event completion, stores result in device struct. */
int MQ136_Process(MQ136Device *ptdev, float temperature, float humidity);

/* Peek at last completed breath event result. Returns 0 if no event yet. */
int MQ136_GetResult(MQ136Device *ptdev, float *ppb, float *confidence,
                    BreathLevel *level);

/* Force median-baseline recalibration (~10s). Caller ensures clean air. */
int MQ136_Recalibrate(MQ136Device *ptdev);

/* Get baseline drift ratio (current_ro / initial_ro).
   > 1.15 = significant upward drift, < 0.85 = significant downward drift. */
float MQ136_GetBaselineDrift(MQ136Device *ptdev);

/* Get current state machine state (for debug telemetry). */
BreathState MQ136_GetState(MQ136Device *ptdev);

/* --- Stateless utility functions --- */

/* Temperature & humidity compensated Rs/Ro ratio */
float mq136_compensate_rs_ro(float rs_ro_raw, float temperature, float humidity);

/* Convert compensated Rs/Ro to ppb H2S (power law model) */
float mq136_rsro_to_ppb(float rs_ro_compensated);

/* Classify ppb H2S into breath quality level */
BreathLevel mq136_ppb_to_level(float ppb_h2s);

/* Deprecated (kept for backward compat) */
float mq136_compensate_rs_ro_old(float rs_ro_raw, float temperature, float humidity);
BreathLevel mq136_classify_compensated(float rs_ro_raw, float temperature, float humidity);

#endif /* __DEV_MQ136_H */
