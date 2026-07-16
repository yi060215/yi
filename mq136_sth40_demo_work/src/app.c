#include "app.h"
#include <devices.h>
#include <drivers.h>
#include <errno.h>
#include <libs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SMELL_BASELINE_MS             3000U
#define SMELL_BREATH_MS               5000U
#define SMELL_SAMPLE_PERIOD_MS         250U
#define SMELL_BASELINE_MIN_SAMPLES       4U
#define SMELL_BREATH_MIN_SAMPLES         8U
#define SMELL_FILTER_NUMERATOR           3U
#define SMELL_FILTER_DENOMINATOR         4U
#define SMELL_DELTA_RAW_THRESHOLD      250U
#define SMELL_DELTA_PERCENT             12U
#define SMELL_DELTA_MIN_RELATIVE_RAW    80U
#define SMELL_ADC_FULL_SCALE          4095U
#define SMELL_ADC_REF_MV              3300U
#define SMELL_ADC_CHANNEL                1U
#define MQ136_WARMUP_SECONDS           60U
#define SESSION_IDLE_SECONDS           60U

typedef struct st_smell_env_snapshot
{
    float temperature_c;
    float humidity_rh;
    bool valid;
    uint32_t sample_count;
    int last_error;
} smell_env_snapshot_t;

typedef struct st_smell_session_summary
{
    uint16_t baseline_raw;
    uint16_t breath_average_raw;
    uint16_t breath_peak_raw;
    uint16_t final_raw;
    uint16_t delta_raw;
    uint16_t relative_threshold_raw;
    uint16_t effective_threshold_raw;
    uint16_t baseline_count;
    uint16_t breath_count;
    float baseline_temp_c;
    float baseline_rh;
    float breath_temp_c;
    float breath_rh;
    bool bad_breath;
} smell_session_summary_t;

static bool sht40_prepare(smell_env_snapshot_t *p_env);
static void sht40_sample(smell_env_snapshot_t *p_env);
static uint16_t mq136_filter_raw(uint16_t previous_filtered, bool has_previous, uint16_t raw);
static uint32_t mq136_raw_to_mv(uint16_t filtered_raw);
static bool mq136_read_raw(ADCDevice *p_adc, uint16_t *p_raw, int *p_error);
static void smell_log_sample(uint32_t t_ms,
                             uint16_t raw,
                             uint16_t filtered,
                             smell_env_snapshot_t const *p_env,
                             uint32_t mq_sample_count,
                             int adc_error);
static void smell_log_session(smell_session_summary_t const *p_summary);
static bool smell_run_session(ADCDevice *p_adc,
                              smell_env_snapshot_t *p_env,
                              uint32_t session_index,
                              smell_session_summary_t *p_summary);
static void smell_print_session_banner(uint32_t session_index);

void DeviceTest(void)
{
    smell_env_snapshot_t env;
    smell_session_summary_t summary;
    ADCDevice *p_mq136_adc;
    uint32_t session_index = 0U;

    memset(&env, 0, sizeof(env));
    memset(&summary, 0, sizeof(summary));
    env.temperature_c = 25.0f;
    env.humidity_rh = 50.0f;

    UartDevicesRegister();
    IODevicesRegister();
    TimerDevicesRegister();
    I2CDevicesRegister();
    ADCDevicesRegister();

    xprintf("\r\n======== MQ136 + SHT40 Smell Session Demo ========\r\n");
    xprintf("Method: same session logic as borrowed smell code\r\n");
    xprintf("Rule: baseline 3s + breath 5s + delta threshold\r\n\r\n");

    (void) sht40_prepare(&env);

    p_mq136_adc = ADCDeviceFind("MQ136 ADC");
    if (NULL == p_mq136_adc)
    {
        xprintf("MQ136: ADC device not found\r\n");
        return;
    }
    if (ESUCCESS != p_mq136_adc->Init(p_mq136_adc))
    {
        xprintf("MQ136: ADC init failed\r\n");
        return;
    }

    xprintf("MQ136: warm-up %u s\r\n", (unsigned int) MQ136_WARMUP_SECONDS);
    for (uint32_t i = 0U; i < MQ136_WARMUP_SECONDS; i++)
    {
        mdelay(1000);
        if (((i + 1U) % 15U) == 0U)
        {
            xprintf("  warm-up: %u/%u s\r\n",
                    (unsigned int) (i + 1U),
                    (unsigned int) MQ136_WARMUP_SECONDS);
        }
    }

    while (true)
    {
        session_index++;
        smell_print_session_banner(session_index);
        if (!smell_run_session(p_mq136_adc, &env, session_index, &summary))
        {
            xprintf("SESSION %lu failed, retry after %u s\r\n",
                    (unsigned long) session_index,
                    (unsigned int) SESSION_IDLE_SECONDS);
        }
        else
        {
            xprintf("SESSION %lu result: %s\r\n",
                    (unsigned long) session_index,
                    summary.bad_breath ? "BAD_BREATH" : "NORMAL");
            xprintf("SESSION %lu next retry in %u s\r\n",
                    (unsigned long) session_index,
                    (unsigned int) SESSION_IDLE_SECONDS);
        }

        for (uint32_t i = 0U; i < SESSION_IDLE_SECONDS; i++)
        {
            mdelay(1000);
        }
    }
}

static bool sht40_prepare(smell_env_snapshot_t *p_env)
{
    uint32_t serial = 0U;

    if (NULL == p_env)
    {
        return false;
    }

    xprintf("--- SHT40 ---\r\n");
    mdelay(10);
    if (!sht40_reset())
    {
        p_env->valid = false;
        p_env->last_error = -EIO;
        xprintf("SHT40: reset FAIL\r\n");
        return false;
    }
    mdelay(10);
    if (sht40_read_serial(&serial))
    {
        xprintf("SHT40: Serial %08lX\r\n", (unsigned long) serial);
        return true;
    }

    p_env->valid = false;
    p_env->last_error = -EIO;
    xprintf("SHT40: Serial read FAIL\r\n");
    return false;
}

static void sht40_sample(smell_env_snapshot_t *p_env)
{
    sht40_data_t data;

    if (NULL == p_env)
    {
        return;
    }

    if (sht40_measure(&data))
    {
        p_env->temperature_c = data.temperature;
        p_env->humidity_rh = data.humidity;
        p_env->valid = true;
        p_env->sample_count++;
        p_env->last_error = ESUCCESS;
    }
    else
    {
        p_env->valid = false;
        p_env->last_error = -EIO;
    }
}

static uint16_t mq136_filter_raw(uint16_t previous_filtered, bool has_previous, uint16_t raw)
{
    uint32_t filtered;

    if (!has_previous)
    {
        return raw;
    }

    filtered = (((uint32_t) previous_filtered * SMELL_FILTER_NUMERATOR) + raw +
                (SMELL_FILTER_DENOMINATOR / 2U)) / SMELL_FILTER_DENOMINATOR;
    if (filtered > UINT16_MAX)
    {
        filtered = UINT16_MAX;
    }

    return (uint16_t) filtered;
}

static uint32_t mq136_raw_to_mv(uint16_t filtered_raw)
{
    return (((uint32_t) filtered_raw * SMELL_ADC_REF_MV) + (SMELL_ADC_FULL_SCALE / 2U)) /
           SMELL_ADC_FULL_SCALE;
}

static bool mq136_read_raw(ADCDevice *p_adc, uint16_t *p_raw, int *p_error)
{
    unsigned short raw = 0U;
    int ret;

    if ((NULL == p_adc) || (NULL == p_raw))
    {
        if (NULL != p_error)
        {
            *p_error = -EINVAL;
        }
        return false;
    }

    ret = p_adc->Read(p_adc, &raw);
    if (ESUCCESS != ret)
    {
        if (NULL != p_error)
        {
            *p_error = ret;
        }
        return false;
    }

    *p_raw = (uint16_t) raw;
    if (NULL != p_error)
    {
        *p_error = ESUCCESS;
    }
    return true;
}

static void smell_log_sample(uint32_t t_ms,
                             uint16_t raw,
                             uint16_t filtered,
                             smell_env_snapshot_t const *p_env,
                             uint32_t mq_sample_count,
                             int adc_error)
{
    uint32_t mv = mq136_raw_to_mv(filtered);
    uint32_t sht_samples = (NULL != p_env) ? p_env->sample_count : 0U;
    int i2c_error = (NULL != p_env) ? p_env->last_error : -EINVAL;
    float temperature = (NULL != p_env) ? p_env->temperature_c : 0.0f;
    float humidity = (NULL != p_env) ? p_env->humidity_rh : 0.0f;
    unsigned int sht_valid = ((NULL != p_env) && p_env->valid) ? 1U : 0U;

    xprintf("SMELL_LOG,t_ms=%lu,raw=%u,filtered=%u,mv=%lu,temp_c=%.2f,rh=%.2f,mq_valid=1,sht_valid=%u,"
            "samples=%lu,sht_samples=%lu,adc_ch=%u,adc_err=%ld,iic_err=%ld\r\n",
            (unsigned long) t_ms,
            (unsigned int) raw,
            (unsigned int) filtered,
            (unsigned long) mv,
            (double) temperature,
            (double) humidity,
            sht_valid,
            (unsigned long) mq_sample_count,
            (unsigned long) sht_samples,
            (unsigned int) SMELL_ADC_CHANNEL,
            (long) adc_error,
            (long) i2c_error);
}

static void smell_log_session(smell_session_summary_t const *p_summary)
{
    if (NULL == p_summary)
    {
        return;
    }

    xprintf("SMELL_SESSION,baseline=%u,breath_avg=%u,breath_peak=%u,final=%u,delta=%u,abs_th=%u,rel_th=%u,"
            "base_temp_c=%.2f,base_rh=%.2f,breath_temp_c=%.2f,breath_rh=%.2f,bad=%u\r\n",
            (unsigned int) p_summary->baseline_raw,
            (unsigned int) p_summary->breath_average_raw,
            (unsigned int) p_summary->breath_peak_raw,
            (unsigned int) p_summary->final_raw,
            (unsigned int) p_summary->delta_raw,
            (unsigned int) SMELL_DELTA_RAW_THRESHOLD,
            (unsigned int) p_summary->relative_threshold_raw,
            (double) p_summary->baseline_temp_c,
            (double) p_summary->baseline_rh,
            (double) p_summary->breath_temp_c,
            (double) p_summary->breath_rh,
            p_summary->bad_breath ? 1U : 0U);
}

static bool smell_run_session(ADCDevice *p_adc,
                              smell_env_snapshot_t *p_env,
                              uint32_t session_index,
                              smell_session_summary_t *p_summary)
{
    uint32_t baseline_sum = 0U;
    uint32_t breath_sum = 0U;
    uint32_t mq_sample_count = 0U;
    float baseline_temp_sum = 0.0f;
    float baseline_rh_sum = 0.0f;
    float breath_temp_sum = 0.0f;
    float breath_rh_sum = 0.0f;
    uint16_t filtered = 0U;
    bool has_filtered = false;
    int adc_error = ESUCCESS;

    if ((NULL == p_adc) || (NULL == p_summary))
    {
        return false;
    }

    memset(p_summary, 0, sizeof(*p_summary));

    xprintf("SESSION %lu: baseline phase 3 s, keep room air stable\r\n",
            (unsigned long) session_index);

    for (uint32_t t_ms = 0U; t_ms < SMELL_BASELINE_MS; t_ms += SMELL_SAMPLE_PERIOD_MS)
    {
        uint16_t raw = 0U;

        sht40_sample(p_env);
        if (!mq136_read_raw(p_adc, &raw, &adc_error))
        {
            xprintf("SESSION %lu: ADC read FAIL in baseline\r\n", (unsigned long) session_index);
            return false;
        }

        filtered = mq136_filter_raw(filtered, has_filtered, raw);
        has_filtered = true;
        baseline_sum += filtered;
        if (p_summary->baseline_count < UINT16_MAX)
        {
            p_summary->baseline_count++;
        }
        baseline_temp_sum += p_env->temperature_c;
        baseline_rh_sum += p_env->humidity_rh;
        mq_sample_count++;
        smell_log_sample(t_ms, raw, filtered, p_env, mq_sample_count, adc_error);
        mdelay(SMELL_SAMPLE_PERIOD_MS);
    }

    if (p_summary->baseline_count < SMELL_BASELINE_MIN_SAMPLES)
    {
        xprintf("SESSION %lu: baseline sample count too low\r\n", (unsigned long) session_index);
        return false;
    }

    p_summary->baseline_raw = (uint16_t) (baseline_sum / p_summary->baseline_count);
    p_summary->baseline_temp_c = baseline_temp_sum / (float) p_summary->baseline_count;
    p_summary->baseline_rh = baseline_rh_sum / (float) p_summary->baseline_count;
    xprintf("SESSION %lu: breath phase 5 s, exhale toward sensor now\r\n",
            (unsigned long) session_index);

    for (uint32_t t_ms = SMELL_BASELINE_MS;
         t_ms < (SMELL_BASELINE_MS + SMELL_BREATH_MS);
         t_ms += SMELL_SAMPLE_PERIOD_MS)
    {
        uint16_t raw = 0U;

        sht40_sample(p_env);
        if (!mq136_read_raw(p_adc, &raw, &adc_error))
        {
            xprintf("SESSION %lu: ADC read FAIL in breath\r\n", (unsigned long) session_index);
            return false;
        }

        filtered = mq136_filter_raw(filtered, has_filtered, raw);
        breath_sum += filtered;
        if (filtered > p_summary->breath_peak_raw)
        {
            p_summary->breath_peak_raw = filtered;
        }
        if (p_summary->breath_count < UINT16_MAX)
        {
            p_summary->breath_count++;
        }
        breath_temp_sum += p_env->temperature_c;
        breath_rh_sum += p_env->humidity_rh;
        mq_sample_count++;
        smell_log_sample(t_ms, raw, filtered, p_env, mq_sample_count, adc_error);
        mdelay(SMELL_SAMPLE_PERIOD_MS);
    }

    if (p_summary->breath_count < SMELL_BREATH_MIN_SAMPLES)
    {
        xprintf("SESSION %lu: breath sample count too low\r\n", (unsigned long) session_index);
        return false;
    }

    p_summary->breath_average_raw = (uint16_t) (breath_sum / p_summary->breath_count);
    p_summary->breath_temp_c = breath_temp_sum / (float) p_summary->breath_count;
    p_summary->breath_rh = breath_rh_sum / (float) p_summary->breath_count;
    p_summary->final_raw = (uint16_t) (((uint32_t) p_summary->breath_average_raw +
                                        p_summary->breath_peak_raw + 1U) / 2U);
    p_summary->delta_raw = (p_summary->final_raw > p_summary->baseline_raw) ?
                           (uint16_t) (p_summary->final_raw - p_summary->baseline_raw) : 0U;
    p_summary->relative_threshold_raw =
        (uint16_t) ((((uint32_t) p_summary->baseline_raw * SMELL_DELTA_PERCENT) + 99U) / 100U);
    if (p_summary->relative_threshold_raw < SMELL_DELTA_MIN_RELATIVE_RAW)
    {
        p_summary->relative_threshold_raw = SMELL_DELTA_MIN_RELATIVE_RAW;
    }
    p_summary->effective_threshold_raw =
        (p_summary->relative_threshold_raw < SMELL_DELTA_RAW_THRESHOLD) ?
        p_summary->relative_threshold_raw : SMELL_DELTA_RAW_THRESHOLD;
    p_summary->bad_breath =
        (p_summary->delta_raw >= SMELL_DELTA_RAW_THRESHOLD) ||
        (p_summary->delta_raw >= p_summary->relative_threshold_raw);

    smell_log_session(p_summary);
    xprintf("SESSION %lu summary: baseline=%u final=%u delta=%u effective_th=%u base_rh=%.2f breath_rh=%.2f result=%s\r\n",
            (unsigned long) session_index,
            (unsigned int) p_summary->baseline_raw,
            (unsigned int) p_summary->final_raw,
            (unsigned int) p_summary->delta_raw,
            (unsigned int) p_summary->effective_threshold_raw,
            (double) p_summary->baseline_rh,
            (double) p_summary->breath_rh,
            p_summary->bad_breath ? "BAD_BREATH" : "NORMAL");
    return true;
}

static void smell_print_session_banner(uint32_t session_index)
{
    xprintf("\r\n========================================\r\n");
    xprintf("SMELL SESSION #%lu\r\n", (unsigned long) session_index);
    xprintf("Pins: MQ136 AO=P001(AN001), SHT40 SCL=P204 SDA=P414, LOG UART7 115200\r\n");
    xprintf("Threshold rule: delta >= 250 raw OR delta >= max(80, ceil(12%% of baseline))\r\n");
    xprintf("========================================\r\n");
}
