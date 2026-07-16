#ifndef __DEV_ADC_H
#define __DEV_ADC_H

typedef struct ADCDev {
    char *name;
    unsigned char channel;
    int (*Init)(struct ADCDev *ptdev);
    int (*Read)(struct ADCDev *ptdev, unsigned short *value);
    struct ADCDev *next;
} ADCDevice;

void ADCDevicesRegister(void);
void ADCDeviceInsert(struct ADCDev *ptdev);
struct ADCDev *ADCDeviceFind(const char *name);
void ADCDeviceList(void);

#endif /* __DEV_ADC_H */
