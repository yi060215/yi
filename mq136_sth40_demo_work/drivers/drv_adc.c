#include "drv_adc.h"
#include <devices.h>
#include <errno.h>
#include <libs.h>
#include <string.h>
#include "hal_data.h"

static int ADCDrvInit(struct ADCDev *ptdev);
static int ADCDrvRead(struct ADCDev *ptdev, unsigned short *value);

static struct ADCDev gADCDevMQ136 = {
    .name    = "MQ136 ADC",
    .channel = 1,
    .Init    = ADCDrvInit,
    .Read    = ADCDrvRead,
    .next    = NULL
};

void ADCDevicesCreate(void)
{
    ADCDeviceInsert(&gADCDevMQ136);
}

static int ADCDrvInit(struct ADCDev *ptdev)
{
    if (NULL == ptdev) return -EINVAL;

    fsp_err_t err = g_adc0.p_api->open(g_adc0.p_ctrl, g_adc0.p_cfg);
    if (FSP_SUCCESS != err) return -EIO;

    /* Configure scan for channel 1 (AN001/P001).
       Create a local copy because the original channel_cfg may have scan_mask=0
       if the channel wasn't fully configured in FSP configurator. */
    adc_channel_cfg_t ch_cfg;
    memcpy(&ch_cfg, g_adc0.p_channel_cfg, sizeof(ch_cfg));
    ch_cfg.scan_mask = (1 << ADC_CHANNEL_1);
    err = g_adc0.p_api->scanCfg(g_adc0.p_ctrl, &ch_cfg);
    if (FSP_SUCCESS != err) return -EIO;

    return ESUCCESS;
}

static int ADCDrvRead(struct ADCDev *ptdev, unsigned short *value)
{
    if (NULL == ptdev || NULL == value) return -EINVAL;

    uint16_t adc_data = 0;

    fsp_err_t err = g_adc0.p_api->scanStart(g_adc0.p_ctrl);
    if (FSP_SUCCESS != err) return -EIO;

    adc_status_t status = {0};
    do {
        err = g_adc0.p_api->scanStatusGet(g_adc0.p_ctrl, &status);
        if (FSP_SUCCESS != err) return -EIO;
    } while (status.state != ADC_STATE_IDLE);

    err = g_adc0.p_api->read(g_adc0.p_ctrl, (adc_channel_t)ptdev->channel, &adc_data);
    if (FSP_SUCCESS != err) return -EIO;

    *value = adc_data;
    return ESUCCESS;
}
