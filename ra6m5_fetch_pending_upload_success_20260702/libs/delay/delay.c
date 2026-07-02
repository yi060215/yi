#include "delay.h"
#include "hal_data.h"

void mdelay(unsigned long msecs)
{
    while (msecs--) {
        R_BSP_SoftwareDelay(1000, BSP_DELAY_UNITS_MICROSECONDS);
    }
}
