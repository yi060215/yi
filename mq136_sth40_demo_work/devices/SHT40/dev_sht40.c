#include "dev_sht40.h"
#include <devices.h>
#include <errno.h>
#include <libs.h>
#include <stddef.h>

static struct I2CDev *gSht40I2C;

/* CRC-8 polynomial 0x31, init 0xFF (Sensirion specific) */
static uint8_t sht40_crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0xFF;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80)
                crc = (uint8_t)((crc << 1) ^ 0x31);
            else
                crc <<= 1;
        }
    }
    return crc;
}

static struct I2CDev *sht40_get_device(void)
{
    if (NULL == gSht40I2C) {
        gSht40I2C = I2CDeviceFind("SHT40");
    }
    return gSht40I2C;
}

static bool sht40_send_command(uint8_t cmd)
{
    struct I2CDev *dev = sht40_get_device();
    if (NULL == dev) return false;

    int ret = dev->Write(dev, &cmd, 1);
    return (ret == ESUCCESS);
}

bool sht40_reset(void)
{
    return sht40_send_command(SHT40_CMD_SOFT_RESET);
}

bool sht40_read_serial(uint32_t *serial)
{
    struct I2CDev *dev = sht40_get_device();
    if (NULL == dev || NULL == serial) return false;

    uint8_t buf[6];
    uint8_t cmd = (uint8_t)SHT40_CMD_READ_SERIAL;

    /* SHT40 serial read requires a full STOP between command and read */
    int ret = dev->Write(dev, &cmd, 1);
    if (ret != ESUCCESS) return false;

    mdelay(10);

    ret = dev->Read(dev, buf, 6);
    if (ret != ESUCCESS) return false;

    /* Verify CRC for each 2-byte pair */
    if (sht40_crc8(&buf[0], 2) != buf[2]) return false;
    if (sht40_crc8(&buf[3], 2) != buf[5]) return false;

    *serial = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16)
            | ((uint32_t)buf[3] << 8)  |  (uint32_t)buf[4];

    return true;
}

bool sht40_measure(sht40_data_t *data)
{
    return sht40_measure_high_precision(data);
}

bool sht40_measure_high_precision(sht40_data_t *data)
{
    struct I2CDev *dev = sht40_get_device();
    if (NULL == dev || NULL == data) return false;

    /* Send measurement command (1 byte) */
    uint8_t cmd = (uint8_t)SHT40_CMD_MEASURE_HIGH;

    int ret = dev->Write(dev, &cmd, 1);
    if (ret != ESUCCESS) return false;

    /* Wait for measurement (high precision: max 8.3ms) */
    mdelay(10);

    /* Read 6 bytes: T_MSB, T_LSB, T_CRC, H_MSB, H_LSB, H_CRC */
    uint8_t rbuf[6];
    ret = dev->Read(dev, rbuf, 6);
    if (ret != ESUCCESS) return false;

    /* Verify CRC */
    if (sht40_crc8(&rbuf[0], 2) != rbuf[2]) return false;
    if (sht40_crc8(&rbuf[3], 2) != rbuf[5]) return false;

    /* Convert to physical values */
    uint16_t t_ticks = (uint16_t)(((uint16_t)rbuf[0] << 8) | rbuf[1]);
    uint16_t h_ticks = (uint16_t)(((uint16_t)rbuf[3] << 8) | rbuf[4]);

    data->temperature = -45.0f + 175.0f * (float)t_ticks / 65535.0f;
    data->humidity    =  -6.0f + 125.0f * (float)h_ticks / 65535.0f;

    if (data->humidity > 100.0f) data->humidity = 100.0f;
    if (data->humidity <   0.0f) data->humidity =   0.0f;

    return true;
}

bool sht40_heat(unsigned char cmd)
{
    return sht40_send_command(cmd);
}
