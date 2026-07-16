#include "drv_i2c.h"
#include "drv_i2c_sw.h"
#include "hal_data.h"
#include <devices.h>
#include <errno.h>
#include <libs.h>
#include <string.h>

static int I2CDrvInit(struct I2CDev *ptdev);
static int I2CDrvWrite(struct I2CDev *ptdev, const unsigned char *buf, unsigned int length);
static int I2CDrvRead(struct I2CDev *ptdev, unsigned char *buf, unsigned int length);
static int I2CDrvWriteRead(struct I2CDev *ptdev, const unsigned char *wbuf, unsigned int wlen,
                           unsigned char *rbuf, unsigned int rlen);

static struct I2CDev gI2CDevice = {
    .name      = "MAX30102",
    .addr      = 0x57,
    .Init      = I2CDrvInit,
    .Write     = I2CDrvWrite,
    .Read      = I2CDrvRead,
    .WriteRead = I2CDrvWriteRead,
    .next      = NULL
};

static struct I2CDev gI2CSHT40 = {
    .name      = "SHT40",
    .addr      = 0x44,
    .Init      = I2CDrvInit,
    .Write     = I2CDrvWrite,
    .Read      = I2CDrvRead,
    .WriteRead = I2CDrvWriteRead,
    .next      = NULL
};

void I2CDevicesCreate(void)
{
    I2CDeviceInsert(&gI2CDevice);
    gI2CDevice.Init(&gI2CDevice);
    I2CDeviceInsert(&gI2CSHT40);
    gI2CSHT40.Init(&gI2CSHT40);
}

static int I2CDrvInit(struct I2CDev *ptdev)
{
    (void)ptdev;
    sw_i2c_init();
    return ESUCCESS;
}

static int I2CDrvWrite(struct I2CDev *ptdev, const unsigned char *buf, unsigned int length)
{
    if (NULL == ptdev)   return -EINVAL;
    if (NULL == buf)     return -EINVAL;
    if (0 == length)     return -EINVAL;

    if (!sw_i2c_write(ptdev->addr, buf, length)) return -EIO;
    return ESUCCESS;
}

static int I2CDrvRead(struct I2CDev *ptdev, unsigned char *buf, unsigned int length)
{
    if (NULL == ptdev)   return -EINVAL;
    if (NULL == buf)     return -EINVAL;
    if (0 == length)     return -EINVAL;

    if (!sw_i2c_read(ptdev->addr, buf, length)) return -EIO;
    return ESUCCESS;
}

static int I2CDrvWriteRead(struct I2CDev *ptdev, const unsigned char *wbuf, unsigned int wlen,
                           unsigned char *rbuf, unsigned int rlen)
{
    if (NULL == ptdev)   return -EINVAL;
    if (NULL == wbuf)    return -EINVAL;
    if (NULL == rbuf)    return -EINVAL;
    if (0 == wlen)       return -EINVAL;
    if (0 == rlen)       return -EINVAL;

    if (!sw_i2c_write_read(ptdev->addr, wbuf, wlen, rbuf, rlen)) return -EIO;
    return ESUCCESS;
}

/* Stub — hardware I2C is not used, but hal_data references this symbol */
void i2c2_callback(i2c_master_callback_args_t *p_args)
{
    (void)p_args;
}
