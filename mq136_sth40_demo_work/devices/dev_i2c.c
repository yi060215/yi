#include "dev_i2c.h"
#include <drivers.h>
#include <libs.h>
#include <string.h>
#include <stdio.h>

static struct I2CDev *gHeadI2CDev;

void I2CDevicesRegister(void)
{
#if DRV_USE_I2C
    I2CDevicesCreate();
#endif
    I2CDeviceList();
}

void I2CDeviceInsert(struct I2CDev *ptdev)
{
    if (NULL == gHeadI2CDev)
        gHeadI2CDev = ptdev;
    else
    {
        ptdev->next = gHeadI2CDev;
        gHeadI2CDev = ptdev;
    }
}

struct I2CDev *I2CDeviceFind(const char *name)
{
    struct I2CDev *ptdev = gHeadI2CDev;
    while (ptdev)
    {
        if (strstr(ptdev->name, name))
            return ptdev;
        ptdev = ptdev->next;
    }
    return NULL;
}

void I2CDeviceList(void)
{
    struct I2CDev *ptdev = gHeadI2CDev;
    xprintf("\r\nI2C Device List:\r\n");
    while (ptdev)
    {
        xprintf("\t%s\r\n", ptdev->name);
        ptdev = ptdev->next;
    }
    xprintf("\r\n");
}
