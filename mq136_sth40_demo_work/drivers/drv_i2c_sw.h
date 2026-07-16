#ifndef __DRV_I2C_SW_H
#define __DRV_I2C_SW_H

#include <stdint.h>
#include <stdbool.h>

void sw_i2c_init(void);
bool sw_i2c_write(uint8_t dev_addr, const uint8_t *buf, uint32_t len);
bool sw_i2c_read(uint8_t dev_addr, uint8_t *buf, uint32_t len);
bool sw_i2c_write_read(uint8_t dev_addr, const uint8_t *wbuf, uint32_t wlen,
                       uint8_t *rbuf, uint32_t rlen);

#endif
