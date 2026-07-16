#ifndef __DEV_I2C_H
#define __DEV_I2C_H

#include <stdint.h>

typedef struct I2CDev {
    char *name;
    uint8_t addr;       /* 7-bit I2C slave address */
    int (*Init)(struct I2CDev *ptdev);
    int (*Write)(struct I2CDev *ptdev, const unsigned char *buf, unsigned int length);
    int (*Read)(struct I2CDev *ptdev, unsigned char *buf, unsigned int length);
    int (*WriteRead)(struct I2CDev *ptdev, const unsigned char *wbuf, unsigned int wlen,
                     unsigned char *rbuf, unsigned int rlen);
    struct I2CDev *next;
} I2CDevice;

void I2CDevicesRegister(void);
void I2CDeviceInsert(struct I2CDev *ptdev);
struct I2CDev *I2CDeviceFind(const char *name);
void I2CDeviceList(void);

#endif /* __DEV_I2C_H */
