#ifndef __DEV_SPI_H
#define __DEV_SPI_H

typedef struct SPIDev{
    char *name;
    unsigned char channel;
    int (*Init)(struct SPIDev *ptdev);
    int (*Write)(struct SPIDev *ptdev, const unsigned char *buf, unsigned int length);
    int (*Read)(struct SPIDev *ptdev, unsigned char *buf, unsigned int length);
    int (*WriteRead)(struct SPIDev *ptdev, unsigned char * const wbuf, unsigned char *rbuf, unsigned int length);
    struct SPIDev *next;
}SPIDevice;

void SPIDevicesRegister(void);
void SPIDeviceInsert(struct SPIDev *ptdev);
struct SPIDev *SPIDeviceFind(const char *name);
void SPIDeviceList(void);

#endif /* __DEV_SPI_H */
