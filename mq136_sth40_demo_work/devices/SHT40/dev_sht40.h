#ifndef __DEV_SHT40_H
#define __DEV_SHT40_H

#include <stdint.h>
#include <stdbool.h>

/* SHT40 I2C slave address (7-bit) */
#define SHT40_I2C_ADDR          0x44

/* SHT40 commands (8-bit) */
#define SHT40_CMD_MEASURE_HIGH    0xFD  /* High precision T&RH, max 8.3ms */
#define SHT40_CMD_MEASURE_MED     0xF6  /* Medium precision T&RH, max 4.5ms */
#define SHT40_CMD_MEASURE_LOW     0xE0  /* Low precision T&RH, max 1.7ms */
#define SHT40_CMD_SOFT_RESET      0x94  /* Soft reset */
#define SHT40_CMD_READ_SERIAL     0x89  /* Read serial number */
#define SHT40_CMD_HEATER_200MW_1S 0x39  /* Heater 200mW, 1s */
#define SHT40_CMD_HEATER_200MW_01S 0x32 /* Heater 200mW, 0.1s */
#define SHT40_CMD_HEATER_110MW_1S 0x2F  /* Heater 110mW, 1s */
#define SHT40_CMD_HEATER_110MW_01S 0x24 /* Heater 110mW, 0.1s */
#define SHT40_CMD_HEATER_20MW_1S  0x1E  /* Heater 20mW, 1s */
#define SHT40_CMD_HEATER_20MW_01S 0x15  /* Heater 20mW, 0.1s */

/* Measurement result */
typedef struct {
    float temperature;   /* °C */
    float humidity;      /* %RH */
} sht40_data_t;

/* Public API */
bool sht40_reset(void);
bool sht40_read_serial(uint32_t *serial);
bool sht40_measure(sht40_data_t *data);
bool sht40_measure_high_precision(sht40_data_t *data);
bool sht40_heat(unsigned char cmd);

#endif /* __DEV_SHT40_H */
