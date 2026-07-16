#include "dev_max30102.h"
#include <devices.h>
#include <errno.h>
#include <libs.h>
#include <stdio.h>
#include <string.h>

static struct I2CDev *gMax30102I2C;

bool max30102_write_reg(uint8_t uch_addr, uint8_t uch_data)
{
    uint8_t buf[2];
    buf[0] = uch_addr;
    buf[1] = uch_data;

    if (NULL == gMax30102I2C)
    {
        gMax30102I2C = I2CDeviceFind("MAX30102");
        if (NULL == gMax30102I2C) return false;
    }

    int ret = gMax30102I2C->Write(gMax30102I2C, buf, 2);
    return (ret == ESUCCESS);
}

bool max30102_read_reg(uint8_t uch_addr, uint8_t *puch_data)
{
    if (NULL == gMax30102I2C)
    {
        gMax30102I2C = I2CDeviceFind("MAX30102");
        if (NULL == gMax30102I2C) return false;
    }

    int ret = gMax30102I2C->WriteRead(gMax30102I2C, &uch_addr, 1, puch_data, 1);
    return (ret == ESUCCESS);
}

bool max30102_reset(void)
{
    return max30102_write_reg(REG_MODE_CONFIG, 0x40);
}

bool max30102_init(void)
{
    gMax30102I2C = I2CDeviceFind("MAX30102");
    if (NULL == gMax30102I2C) return false;

    if (!max30102_write_reg(REG_INTR_ENABLE_1, 0xC0)) return false;
    if (!max30102_write_reg(REG_INTR_ENABLE_2, 0x00)) return false;
    if (!max30102_write_reg(REG_FIFO_WR_PTR, 0x00))   return false;
    if (!max30102_write_reg(REG_OVF_COUNTER, 0x00))   return false;
    if (!max30102_write_reg(REG_FIFO_RD_PTR, 0x00))   return false;
    if (!max30102_write_reg(REG_SPO2_CONFIG, 0x27))   return false;
    if (!max30102_write_reg(REG_FIFO_CONFIG, 0x1F))   return false;
    if (!max30102_write_reg(REG_LED1_PA, 0x18))       return false;
    if (!max30102_write_reg(REG_LED2_PA, 0x18))       return false;
    if (!max30102_write_reg(REG_PILOT_PA, 0x7F))      return false;
    if (!max30102_write_reg(REG_MODE_CONFIG, 0x03))   return false;

    return true;
}

bool max30102_read_fifo(uint32_t *pun_red_led, uint32_t *pun_ir_led)
{
    uint32_t un_temp;
    uint8_t uch_temp;
    uint8_t reg = REG_FIFO_DATA;
    uint8_t buf[6];

    if (NULL == gMax30102I2C) return false;

    /* Clear status registers */
    max30102_read_reg(REG_INTR_STATUS_1, &uch_temp);
    max30102_read_reg(REG_INTR_STATUS_2, &uch_temp);

    /* Read 6 bytes from FIFO */
    int ret = gMax30102I2C->WriteRead(gMax30102I2C, &reg, 1, buf, 6);
    if (ret != ESUCCESS) return false;

    *pun_red_led = 0;
    *pun_ir_led  = 0;

    un_temp = buf[0];
    un_temp <<= 16;
    *pun_red_led += un_temp;
    un_temp = buf[1];
    un_temp <<= 8;
    *pun_red_led += un_temp;
    un_temp = buf[2];
    *pun_red_led += un_temp;

    un_temp = buf[3];
    un_temp <<= 16;
    *pun_ir_led += un_temp;
    un_temp = buf[4];
    un_temp <<= 8;
    *pun_ir_led += un_temp;
    un_temp = buf[5];
    *pun_ir_led += un_temp;

    *pun_red_led &= 0x03FFFF;
    *pun_ir_led  &= 0x03FFFF;

    return true;
}

bool max30102_get_status(uint8_t *status1, uint8_t *status2)
{
    if (!max30102_read_reg(REG_INTR_STATUS_1, status1)) return false;
    if (!max30102_read_reg(REG_INTR_STATUS_2, status2)) return false;
    return true;
}

bool max30102_read_temperature(float *temp)
{
    uint8_t intr, frac;

    if (!max30102_write_reg(REG_TEMP_CONFIG, 0x01)) return false;

    if (!max30102_read_reg(REG_TEMP_INTR, &intr)) return false;
    if (!max30102_read_reg(REG_TEMP_FRAC, &frac)) return false;

    *temp = (float)intr + (float)frac * 0.0625f;
    return true;
}
