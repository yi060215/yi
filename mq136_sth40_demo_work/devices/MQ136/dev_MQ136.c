#include "dev_MQ136.h"
#include <devices.h>
#include <errno.h>
#include <libs.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* ================================================================ */
/*  Internal helpers (forward declarations)                         */
/* ================================================================ */

static int      MQ136DevInit(struct MQ136Dev *ptdev);
static int      MQ136DevCalibrate(struct MQ136Dev *ptdev);
static int      MQ136DevRead(struct MQ136Dev *ptdev);
static int      MQ136DevProcess(struct MQ136Dev *ptdev, float temperature, float humidity);
static float    mq136_filtered_voltage(struct MQ136Dev *ptdev, float new_voltage);
static float    mq136_voltage_to_rs(float voltage);
static void     mq136_compute_event_result(struct MQ136Dev *ptdev);
static float    mq136_compute_confidence(float peak_depth, int duration_ms,
                                         float temperature, float humidity);
static float    mq136_sort_median(float *arr, int n);

/* ---- Public utility (used by static functions, must be declared early) ---- */
float           mq136_compensate_rs_ro(float rs_ro_raw, float temperature, float humidity);
float           mq136_rsro_to_ppb(float rs_ro_compensated);
BreathLevel     mq136_ppb_to_level(float ppb_h2s);

/* ---- Static helpers for idle noise tracking ---- */
static void     idle_noise_update(struct MQ136Dev *ptdev, float rs_ro);
static float    idle_noise_estimate(struct MQ136Dev *ptdev);
static float    idle_noise_min_max_range(float *buf, int count);

/* ================================================================ */
/*  ADC device reference                                            */
/* ================================================================ */

static ADCDevice *pMQ136ADC = NULL;

/* ================================================================ */
/*  Idle noise buffer (shared, not per-device since only one MQ136) */
/* ================================================================ */

#define IDLE_NOISE_BUF_SIZE   50   /* 5 seconds at 100ms */
static float g_idle_rsro_buf[IDLE_NOISE_BUF_SIZE];
static int   g_idle_rsro_idx = 0;
static int   g_idle_rsro_count = 0;

/* ================================================================ */
/*  Singleton device instance                                       */
/* ================================================================ */

static MQ136Device gMQ136 = {
    .name                 = "MQ136",
    .baseline_voltage     = 0.0f,
    .baseline_rs          = 0.0f,
    .initial_baseline_rs  = 0.0f,
    .baseline_frozen      = 0,
    .current_voltage      = 0.0f,
    .current_rs           = 0.0f,
    .rs_ro_ratio          = 1.0f,
    .rs_ro_compensated    = 1.0f,
    .voltage_buffer       = {0},
    .buffer_index         = 0,
    .level                = BREATH_NORMAL,
    .event_state          = BREATH_STATE_IDLE,
    .event_confirm_cnt    = 0,
    .event_sample_cnt     = 0,
    .event_recovery_cnt   = 0,
    .event_peak_rs_ro     = 1.0f,
    .event_peak_temperature = 20.0f,
    .event_peak_humidity    = 65.0f,
    .last_ppb_h2s         = 0.0f,
    .last_confidence      = 0.0f,
    .last_event_valid     = 0,
    .last_event_duration_ms = 0,
    .last_level           = BREATH_NORMAL,
    .Init                 = MQ136DevInit,
    .Calibrate            = MQ136DevCalibrate,
    .Read                 = MQ136DevRead,
    .Process              = MQ136DevProcess,
};

/* ================================================================ */
/*  Public: Get singleton                                           */
/* ================================================================ */

struct MQ136Dev *MQ136GetDevice(void)
{
    return &gMQ136;
}

/* ================================================================ */
/*  Init: hardware setup only                                       */
/* ================================================================ */

static int MQ136DevInit(struct MQ136Dev *ptdev)
{
    if (NULL == ptdev) return -EINVAL;

    pMQ136ADC = ADCDeviceFind("MQ136 ADC");
    if (NULL == pMQ136ADC) return -ENXIO;
    if (ESUCCESS != pMQ136ADC->Init(pMQ136ADC)) return -EIO;

    /* Initialize filter buffer */
    for (int i = 0; i < MQ136_FILTER_WINDOW; i++)
        ptdev->voltage_buffer[i] = 0.0f;
    ptdev->buffer_index = 0;

    /* Initialize state machine */
    ptdev->event_state        = BREATH_STATE_IDLE;
    ptdev->event_confirm_cnt  = 0;
    ptdev->event_sample_cnt   = 0;
    ptdev->event_recovery_cnt = 0;
    ptdev->event_peak_rs_ro   = 1.0f;
    ptdev->last_event_valid   = 0;
    ptdev->baseline_frozen    = 0;

    /* Initialize idle noise buffer */
    g_idle_rsro_idx   = 0;
    g_idle_rsro_count = 0;
    for (int i = 0; i < IDLE_NOISE_BUF_SIZE; i++)
        g_idle_rsro_buf[i] = 1.0f;

    xprintf("MQ136: Initialized OK\r\n");
    return ESUCCESS;
}

/* ================================================================ */
/*  Calibrate: median baseline collection (assumes clean air)       */
/* ================================================================ */

static int MQ136DevCalibrate(struct MQ136Dev *ptdev)
{
    if (NULL == ptdev || NULL == pMQ136ADC) return -EINVAL;

    static float rs_samples[MQ136_CALIB_SAMPLES];
    unsigned short adc_val = 0;

    xprintf("MQ136: Calibrating median baseline (%d samples)...\r\n",
            MQ136_CALIB_SAMPLES);

    /* Discard first few readings (sensor settling) */
    for (int i = 0; i < 10; i++) {
        pMQ136ADC->Read(pMQ136ADC, &adc_val);
        mdelay(10);
    }

    /* Collect samples, convert to Rs */
    for (int i = 0; i < MQ136_CALIB_SAMPLES; i++) {
        pMQ136ADC->Read(pMQ136ADC, &adc_val);
        float voltage = (float)adc_val * MQ136_ADC_VREF / MQ136_ADC_MAX;
        rs_samples[i] = mq136_voltage_to_rs(voltage);
        mdelay(20);
    }

    /* Median is robust to outliers during calibration */
    float median_rs = mq136_sort_median(rs_samples, MQ136_CALIB_SAMPLES);
    ptdev->baseline_rs         = median_rs;
    ptdev->initial_baseline_rs = median_rs;
    ptdev->baseline_voltage    = 0.0f; /* Derived from Rs if needed */
    ptdev->rs_ro_ratio         = 1.0f;
    ptdev->rs_ro_compensated   = 1.0f;
    ptdev->level               = BREATH_NORMAL;
    ptdev->baseline_frozen     = 0;

    /* Seed the filter buffer with baseline-equivalent voltage */
    float v_baseline = MQ136_VC * MQ136_RL_KOHM / (median_rs + MQ136_RL_KOHM);
    for (int i = 0; i < MQ136_FILTER_WINDOW; i++)
        ptdev->voltage_buffer[i] = v_baseline;

    /* Reset idle noise buffer with clean baseline */
    g_idle_rsro_idx   = 0;
    g_idle_rsro_count = 0;
    for (int i = 0; i < IDLE_NOISE_BUF_SIZE; i++)
        g_idle_rsro_buf[i] = 1.0f;

    int ro_int  = (int)(median_rs);
    int ro_frac = (int)(median_rs * 100.0f) % 100;
    xprintf("MQ136: Median baseline Ro=%d.%02dkOhm\r\n", ro_int, ro_frac);
    return ESUCCESS;
}

/* ================================================================ */
/*  Read: legacy per-sample read (no state machine)                 */
/*  Kept for backward compatibility. New code should use Process(). */
/* ================================================================ */

static int MQ136DevRead(struct MQ136Dev *ptdev)
{
    if (NULL == ptdev || NULL == pMQ136ADC) return -EINVAL;

    unsigned short adc_val = 0;
    if (ESUCCESS != pMQ136ADC->Read(pMQ136ADC, &adc_val)) return -EIO;

    float raw_voltage = (float)adc_val * MQ136_ADC_VREF / MQ136_ADC_MAX;
    ptdev->current_voltage = mq136_filtered_voltage(ptdev, raw_voltage);
    ptdev->current_rs = mq136_voltage_to_rs(ptdev->current_voltage);

    if (ptdev->baseline_rs > 0.001f)
        ptdev->rs_ro_ratio = ptdev->current_rs / ptdev->baseline_rs;
    else
        ptdev->rs_ro_ratio = 1.0f;

    return ESUCCESS;
}

/* ================================================================ */
/*  Process: core routine — call every 100ms                        */
/*  1. Read ADC + filter                                            */
/*  2. Compute Rs/Ro, apply T&H compensation                        */
/*  3. Update adaptive baseline (when IDLE + clean air)             */
/*  4. Run breath event state machine                               */
/*  5. On event completion: compute ppb, confidence, level          */
/* ================================================================ */

static int MQ136DevProcess(struct MQ136Dev *ptdev, float temperature, float humidity)
{
    if (NULL == ptdev || NULL == pMQ136ADC) return -EINVAL;

    /* --- Step 1: Read & filter --- */
    unsigned short adc_val = 0;
    if (ESUCCESS != pMQ136ADC->Read(pMQ136ADC, &adc_val)) return -EIO;

    float raw_voltage = (float)adc_val * MQ136_ADC_VREF / MQ136_ADC_MAX;
    ptdev->current_voltage = mq136_filtered_voltage(ptdev, raw_voltage);
    ptdev->current_rs = mq136_voltage_to_rs(ptdev->current_voltage);

    /* --- Step 2: Rs/Ro --- */
    if (ptdev->baseline_rs > 0.001f)
        ptdev->rs_ro_ratio = ptdev->current_rs / ptdev->baseline_rs;
    else
        ptdev->rs_ro_ratio = 1.0f;

    /* T&H compensated Rs/Ro */
    ptdev->rs_ro_compensated = mq136_compensate_rs_ro(
        ptdev->rs_ro_ratio, temperature, humidity);

    /* --- Step 3: State machine --- */
    switch (ptdev->event_state) {

    case BREATH_STATE_IDLE:
        /* Adaptive baseline: slow EMA toward current Rs when air looks clean */
        if (ptdev->rs_ro_ratio >= BASELINE_UPDATE_MIN_RSRO && !ptdev->baseline_frozen) {
            ptdev->baseline_rs = ptdev->baseline_rs * (1.0f - BASELINE_EMA_ALPHA)
                               + ptdev->current_rs * BASELINE_EMA_ALPHA;
        }

        /* Track Rs/Ro during IDLE for noise estimation */
        idle_noise_update(ptdev, ptdev->rs_ro_compensated);

        /* Check trigger */
        if (ptdev->rs_ro_compensated < BED_TRIGGER_RATIO) {
            ptdev->event_state        = BREATH_STATE_RISING_EDGE;
            ptdev->event_confirm_cnt  = 1;
            ptdev->event_sample_cnt   = 1;
            ptdev->event_recovery_cnt = 0;
            ptdev->event_peak_rs_ro   = ptdev->rs_ro_compensated;
            ptdev->event_peak_temperature = temperature;
            ptdev->event_peak_humidity    = humidity;
        }
        break;

    case BREATH_STATE_RISING_EDGE:
        ptdev->event_sample_cnt++;

        if (ptdev->rs_ro_compensated < BED_TRIGGER_RATIO) {
            ptdev->event_confirm_cnt++;
            /* Track min Rs/Ro during rising edge */
            if (ptdev->rs_ro_compensated < ptdev->event_peak_rs_ro) {
                ptdev->event_peak_rs_ro   = ptdev->rs_ro_compensated;
                ptdev->event_peak_temperature = temperature;
                ptdev->event_peak_humidity    = humidity;
            }

            if (ptdev->event_confirm_cnt >= BED_CONFIRM_CNT) {
                /* Confirmed: enter breathing state */
                ptdev->event_state      = BREATH_STATE_BREATHING;
                ptdev->event_recovery_cnt = 0;
                /* event_sample_cnt already includes all samples since trigger */
            }
        } else {
            /* False trigger — noise spike */
            ptdev->event_state = BREATH_STATE_IDLE;
        }
        break;

    case BREATH_STATE_BREATHING:
        ptdev->event_sample_cnt++;

        /* Track minimum Rs/Ro = peak H2S */
        if (ptdev->rs_ro_compensated < ptdev->event_peak_rs_ro) {
            ptdev->event_peak_rs_ro   = ptdev->rs_ro_compensated;
            ptdev->event_peak_temperature = temperature;
            ptdev->event_peak_humidity    = humidity;
        }

        /* Check for recovery: signal rising back above 105% of peak */
        if (ptdev->rs_ro_compensated > ptdev->event_peak_rs_ro * 1.05f) {
            ptdev->event_recovery_cnt++;
            if (ptdev->event_recovery_cnt >= BED_RECOVERY_CNT) {
                ptdev->event_state = BREATH_STATE_FALLING_EDGE;
            }
        } else {
            ptdev->event_recovery_cnt = 0;
        }

        /* Timeout: maximum event duration */
        if (ptdev->event_sample_cnt * 100 >= BED_MAX_DURATION_MS) {
            ptdev->event_state = BREATH_STATE_FALLING_EDGE;
        }
        break;

    case BREATH_STATE_FALLING_EDGE:
        ptdev->event_sample_cnt++;

        /* Recover: signal back near baseline, or forced timeout */
        if (ptdev->rs_ro_compensated > BED_RECOVERY_RATIO ||
            ptdev->event_sample_cnt * 100 >= BED_MAX_DURATION_MS + 5000) {
            mq136_compute_event_result(ptdev);
            ptdev->event_state = BREATH_STATE_IDLE;
        }
        break;
    }

    /* --- Step 4: Live classification (for debug telemetry) --- */
    ptdev->level = mq136_ppb_to_level(
        mq136_rsro_to_ppb(ptdev->rs_ro_compensated));

    return ESUCCESS;
}

/* ================================================================ */
/*  GetResult: peek at last completed event                         */
/* ================================================================ */

int MQ136_GetResult(MQ136Device *ptdev, float *ppb, float *confidence,
                    BreathLevel *level)
{
    if (NULL == ptdev) return -EINVAL;
    if (!ptdev->last_event_valid) return -ENODATA;

    if (ppb)        *ppb        = ptdev->last_ppb_h2s;
    if (confidence) *confidence = ptdev->last_confidence;
    if (level)      *level      = ptdev->last_level;

    /* Mark as consumed */
    ptdev->last_event_valid = 0;
    return ESUCCESS;
}

/* ================================================================ */
/*  Recalibrate: force fresh baseline collection                    */
/* ================================================================ */

int MQ136_Recalibrate(MQ136Device *ptdev)
{
    if (NULL == ptdev) return -EINVAL;

    xprintf("MQ136: Recalibration requested — ensure clean air!\r\n");

    /* Freeze baseline during recal */
    ptdev->baseline_frozen = 1;

    /* Brief settling delay */
    for (int i = 0; i < 10; i++) mdelay(100);

    /* Collect fresh median baseline */
    static float rs_samples[MQ136_CALIB_SAMPLES];
    unsigned short adc_val = 0;

    for (int i = 0; i < MQ136_CALIB_SAMPLES; i++) {
        pMQ136ADC->Read(pMQ136ADC, &adc_val);
        float voltage = (float)adc_val * MQ136_ADC_VREF / MQ136_ADC_MAX;
        rs_samples[i] = mq136_voltage_to_rs(voltage);
        mdelay(20);
    }

    float new_ro = mq136_sort_median(rs_samples, MQ136_CALIB_SAMPLES);
    ptdev->baseline_rs = new_ro;

    /* Update seed voltage buffer */
    float v_baseline = MQ136_VC * MQ136_RL_KOHM / (new_ro + MQ136_RL_KOHM);
    for (int i = 0; i < MQ136_FILTER_WINDOW; i++)
        ptdev->voltage_buffer[i] = v_baseline;

    ptdev->baseline_frozen = 0;
    ptdev->last_event_valid = 0;

    int ro_int  = (int)new_ro;
    int ro_frac = (int)(new_ro * 100.0f) % 100;
    xprintf("MQ136: Recalibrated Ro=%d.%02dkOhm\r\n", ro_int, ro_frac);
    return ESUCCESS;
}

/* ================================================================ */
/*  GetBaselineDrift: ratio of current Ro to initial Ro             */
/* ================================================================ */

float MQ136_GetBaselineDrift(MQ136Device *ptdev)
{
    if (NULL == ptdev || ptdev->initial_baseline_rs < 0.001f)
        return 1.0f;
    return ptdev->baseline_rs / ptdev->initial_baseline_rs;
}

/* ================================================================ */
/*  GetState: current state machine state                           */
/* ================================================================ */

BreathState MQ136_GetState(MQ136Device *ptdev)
{
    if (NULL == ptdev) return BREATH_STATE_IDLE;
    return ptdev->event_state;
}

/* ================================================================ */
/*  Stateless: T&H compensation (improved with cross-term)          */
/* ================================================================ */

float mq136_compensate_rs_ro(float rs_ro_raw, float temperature, float humidity)
{
    float dt = temperature - MQ136_COMP_T_REF;
    float drh = humidity - MQ136_COMP_RH_REF;

    float factor = 1.0f
        + MQ136_COMP_ALPHA * dt
        + MQ136_COMP_BETA  * drh
        + MQ136_COMP_GAMMA * dt * drh;

    /* Clamp to reasonable range */
    if (factor < MQ136_COMP_FACTOR_MIN) factor = MQ136_COMP_FACTOR_MIN;
    if (factor > MQ136_COMP_FACTOR_MAX) factor = MQ136_COMP_FACTOR_MAX;

    return rs_ro_raw / factor;
}

/* ================================================================ */
/*  Stateless: Rs/Ro → ppb H₂S (power law)                         */
/*  MQ136 sensitivity: Rs/Ro = A * ppm^B                            */
/*  => ppm = (A / RsRo) ^ (1/|B|)                                   */
/* ================================================================ */

float mq136_rsro_to_ppb(float rs_ro_compensated)
{
    if (rs_ro_compensated >= 0.99f)
        return 0.0f;  /* Effectively clean air */

    /* Clamp to avoid numerical issues */
    float clamped = rs_ro_compensated;
    if (clamped < 0.03f) clamped = 0.03f;

    float ppm = powf(MQ136_SENS_A / clamped, MQ136_PPB_EXPONENT);
    float ppb = ppm * 1000.0f;

    if (ppb < MQ136_PPB_FLOOR) ppb = 0.0f;
    return ppb;
}

/* ================================================================ */
/*  Stateless: ppb → breath level                                   */
/* ================================================================ */

BreathLevel mq136_ppb_to_level(float ppb_h2s)
{
    if (ppb_h2s < MQ136_PPB_NORMAL)   return BREATH_NORMAL;
    if (ppb_h2s < MQ136_PPB_MILD)     return BREATH_MILD;
    if (ppb_h2s < MQ136_PPB_MODERATE) return BREATH_MODERATE;
    return BREATH_SEVERE;
}

/* ================================================================ */
/*  Deprecated: backward-compatible wrappers                        */
/* ================================================================ */

float mq136_compensate_rs_ro_old(float rs_ro_raw, float temperature, float humidity)
{
    /* Old linear model without cross-term, kept for reference */
    float alpha = -0.015f;
    float beta  = -0.008f;
    float factor = 1.0f
        + alpha * (temperature - 20.0f)
        + beta  * (humidity    - 65.0f);
    if (factor < 0.5f) factor = 0.5f;
    if (factor > 1.5f) factor = 1.5f;
    return rs_ro_raw / factor;
}

BreathLevel mq136_classify_compensated(float rs_ro_raw, float temperature, float humidity)
{
    float corrected = mq136_compensate_rs_ro(rs_ro_raw, temperature, humidity);
    /* Old 3-level classification */
    if (corrected > 0.85f) return BREATH_NORMAL;
    if (corrected > 0.70f) return BREATH_MILD;
    return BREATH_SEVERE;
}

/* ================================================================ */
/*  Internal: Compute event result from captured peak               */
/* ================================================================ */

static void mq136_compute_event_result(struct MQ136Dev *ptdev)
{
    int duration_ms = ptdev->event_sample_cnt * 100;

    /* Reject events shorter than minimum duration */
    if (duration_ms < BED_MIN_DURATION_MS || ptdev->event_peak_rs_ro >= 0.99f) {
        ptdev->last_event_valid = 0;
        return;
    }

    /* event_peak_rs_ro is already T&H compensated (stored as rs_ro_compensated
       during state machine). Use it directly — no double-compensation. */
    float ppb  = mq136_rsro_to_ppb(ptdev->event_peak_rs_ro);
    float peak_depth = 1.0f - ptdev->event_peak_rs_ro;
    float conf = mq136_compute_confidence(
        peak_depth, duration_ms,
        ptdev->event_peak_temperature,
        ptdev->event_peak_humidity);

    ptdev->last_ppb_h2s         = ppb;
    ptdev->last_confidence      = conf;
    ptdev->last_level           = mq136_ppb_to_level(ppb);
    ptdev->last_event_duration_ms = duration_ms;
    ptdev->last_event_valid     = 1;
}

/* ================================================================ */
/*  Internal: Confidence scoring                                    */
/* ================================================================ */

static float mq136_compute_confidence(float peak_depth, int duration_ms,
                                       float temperature, float humidity)
{
    float c_magnitude = 0.0f;
    float c_duration  = 0.0f;
    float c_snr       = 0.0f;
    float c_env       = 1.0f;

    /* Magnitude: deeper peak = higher confidence */
    if (peak_depth > 0.0f) {
        c_magnitude = peak_depth / CONF_PEAK_FULL_DEPTH;
        if (c_magnitude > 1.0f) c_magnitude = 1.0f;
    }

    /* Duration: ideal breath = 2-8 seconds */
    if (duration_ms >= CONF_DUR_IDEAL_MIN && duration_ms <= CONF_DUR_IDEAL_MAX) {
        c_duration = 1.0f;
    } else if (duration_ms >= CONF_DUR_ACCEPT_MIN && duration_ms <= CONF_DUR_ACCEPT_MAX) {
        c_duration = 0.5f;
    } /* else 0.0 */

    /* Signal-to-noise: peak_depth / idle baseline noise range */
    float noise = idle_noise_estimate(NULL);
    if (noise < 0.001f) noise = 0.001f;
    float snr = peak_depth / noise;
    c_snr = snr / CONF_SNR_FULL;
    if (c_snr > 1.0f) c_snr = 1.0f;

    /* Environment: T&H within valid operating range */
    if (temperature < CONF_T_MIN || temperature > CONF_T_MAX) c_env *= 0.5f;
    if (humidity    < CONF_RH_MIN || humidity    > CONF_RH_MAX) c_env *= 0.5f;

    return CONF_W_MAGNITUDE * c_magnitude
         + CONF_W_DURATION  * c_duration
         + CONF_W_SNR       * c_snr
         + CONF_W_ENV       * c_env;
}

/* ================================================================ */
/*  Internal: Moving average filter                                 */
/* ================================================================ */

static float mq136_filtered_voltage(struct MQ136Dev *ptdev, float new_voltage)
{
    ptdev->voltage_buffer[ptdev->buffer_index] = new_voltage;
    ptdev->buffer_index = (ptdev->buffer_index + 1) % MQ136_FILTER_WINDOW;

    float sum = 0.0f;
    for (int i = 0; i < MQ136_FILTER_WINDOW; i++)
        sum += ptdev->voltage_buffer[i];

    return sum / (float)MQ136_FILTER_WINDOW;
}

/* ================================================================ */
/*  Internal: Voltage divider → sensor resistance (kOhm)            */
/* ================================================================ */

static float mq136_voltage_to_rs(float voltage)
{
    if (voltage < 0.005f)
        voltage = 0.005f;
    return MQ136_RL_KOHM * (MQ136_VC / voltage - 1.0f);
}

/* ================================================================ */
/*  Internal: Median of float array (insertion sort, small N)       */
/* ================================================================ */

static float mq136_sort_median(float *arr, int n)
{
    if (n <= 0) return 0.0f;
    if (n == 1) return arr[0];

    /* Insertion sort — efficient for nearly-sorted or small arrays */
    for (int i = 1; i < n; i++) {
        float key = arr[i];
        int j = i - 1;
        while (j >= 0 && arr[j] > key) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }

    return arr[n / 2];
}

/* ================================================================ */
/*  Internal: Idle noise tracking                                   */
/* ================================================================ */

static void idle_noise_update(struct MQ136Dev *ptdev, float rs_ro)
{
    (void)ptdev;
    g_idle_rsro_buf[g_idle_rsro_idx] = rs_ro;
    g_idle_rsro_idx = (g_idle_rsro_idx + 1) % IDLE_NOISE_BUF_SIZE;
    if (g_idle_rsro_count < IDLE_NOISE_BUF_SIZE)
        g_idle_rsro_count++;
}

static float idle_noise_estimate(struct MQ136Dev *ptdev)
{
    (void)ptdev;
    if (g_idle_rsro_count < 10) return 0.01f; /* Not enough data yet */
    return idle_noise_min_max_range(g_idle_rsro_buf, g_idle_rsro_count);
}

static float idle_noise_min_max_range(float *buf, int count)
{
    float min_val = buf[0];
    float max_val = buf[0];
    for (int i = 1; i < count; i++) {
        if (buf[i] < min_val) min_val = buf[i];
        if (buf[i] > max_val) max_val = buf[i];
    }
    return max_val - min_val;
}
