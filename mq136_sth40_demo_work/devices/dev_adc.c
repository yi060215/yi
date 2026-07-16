#include "dev_adc.h"
#include <drivers.h>
#include <libs.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static struct ADCDev *gHeadADCDev;

void ADCDevicesRegister(void)
{
#if DRV_USE_ADC
    ADCDevicesCreate();
#endif
    ADCDeviceList();
}

void ADCDeviceInsert(struct ADCDev *ptdev)
{
    if (NULL == gHeadADCDev)
        gHeadADCDev = ptdev;
    else {
        ptdev->next = gHeadADCDev;
        gHeadADCDev = ptdev;
    }
}

struct ADCDev *ADCDeviceFind(const char *name)
{
    struct ADCDev *ptdev = gHeadADCDev;
    while (ptdev) {
        if (strstr(ptdev->name, name))
            return ptdev;
        ptdev = ptdev->next;
    }
    return NULL;
}

void ADCDeviceList(void)
{
    struct ADCDev *ptdev = gHeadADCDev;
    xprintf("\r\nADC Device List:\r\n");
    while (ptdev) {
        xprintf("\t%s\r\n", ptdev->name);
        ptdev = ptdev->next;
    }
    xprintf("\r\n");
}
