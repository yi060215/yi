#include "drv_i2c_sw.h"
#include "bsp_api.h"

/* P204 = SCL (Port 2, Pin 4),  P414 = SDA (Port 4, Pin 14) */

#define I2C_DELAY_US  5   /* ~100kHz standard mode */

static void delay_us(volatile uint32_t us)
{
    while (us--) {
        for (volatile uint32_t j = 0; j < 40; j++) { __NOP(); }
    }
}

/*
 * RA6M5 PORT registers (via CMSIS R_PORTx structs):
 *   PDR_b.PDRx   → direction (1=output, 0=input) at offset 0x02
 *   POSR_b.POSRx → set output high, atomic write at offset 0x0A
 *   PORR_b.PORRx → set output low,  atomic write at offset 0x08
 *   PIDR_b.PIDRx → read input level at offset 0x04
 */
#define SCL_SET()  (R_PORT2->POSR_b.POSR4 = 1U)
#define SCL_CLR()  (R_PORT2->PORR_b.PORR4 = 1U)
#define SDA_SET()  (R_PORT4->POSR_b.POSR14 = 1U)
#define SDA_CLR()  (R_PORT4->PORR_b.PORR14 = 1U)
#define SDA_OUT()  (R_PORT4->PDR_b.PDR14 = 1U)
#define SDA_IN()   (R_PORT4->PDR_b.PDR14 = 0U)
#define SDA_RD()   (R_PORT4->PIDR_b.PIDR14)

void sw_i2c_init(void)
{
    /*
     * Override PFS to force P204/P414 into GPIO mode.
     * P204 is configured as SPI in pin_data.c, which would block GPIO access.
     */
    R_PMISC->PWPR_b.B0WI  = 0;   /* Unlock PFS write protect */
    R_PMISC->PWPR_b.PFSWE = 1;   /* Enable PFS register write */

    R_PFS->PORT[2].PIN[4].PmnPFS  = (1U << 5);   /* P204 → GPIO, pull-up */
    R_PFS->PORT[4].PIN[14].PmnPFS = (1U << 5);   /* P414 → GPIO, pull-up */

    R_PMISC->PWPR_b.PFSWE = 0;   /* Disable PFS write */
    R_PMISC->PWPR_b.B0WI  = 1;   /* Lock PFS write protect */

    /* Set pin direction and idle state (both high = bus released) */
    R_PORT2->PDR_b.PDR4  = 1U;   /* P204 output */
    R_PORT4->PDR_b.PDR14 = 1U;   /* P414 output */
    SCL_SET();
    SDA_SET();
    delay_us(10);
}

static void i2c_start(void)
{
    SDA_OUT();
    SDA_SET();
    delay_us(I2C_DELAY_US);
    SCL_SET();
    delay_us(I2C_DELAY_US);
    SDA_CLR();
    delay_us(I2C_DELAY_US);
    SCL_CLR();
}

static void i2c_stop(void)
{
    SDA_OUT();
    SDA_CLR();
    delay_us(I2C_DELAY_US);
    SCL_SET();
    delay_us(I2C_DELAY_US);
    SDA_SET();
    delay_us(I2C_DELAY_US);
}

static int i2c_write_byte(uint8_t data)
{
    SDA_OUT();
    for (int i = 0; i < 8; i++)
    {
        if (data & 0x80) SDA_SET(); else SDA_CLR();
        delay_us(I2C_DELAY_US);
        SCL_SET();
        delay_us(I2C_DELAY_US);
        SCL_CLR();
        data <<= 1;
    }
    /* Read ACK from slave */
    SDA_IN();
    delay_us(I2C_DELAY_US);
    SCL_SET();
    delay_us(I2C_DELAY_US);
    int ack = SDA_RD();
    SCL_CLR();
    SDA_OUT();
    return ack;  /* 0 = ACK, 1 = NACK */
}

static uint8_t i2c_read_byte(int ack)
{
    uint8_t data = 0;
    SDA_IN();
    for (int i = 0; i < 8; i++)
    {
        delay_us(I2C_DELAY_US);
        SCL_SET();
        delay_us(I2C_DELAY_US);
        data = (uint8_t)((data << 1) | (SDA_RD() ? 1 : 0));
        SCL_CLR();
    }
    /* Send ACK (0) or NACK (1) to slave */
    SDA_OUT();
    if (ack) SDA_SET(); else SDA_CLR();
    delay_us(I2C_DELAY_US);
    SCL_SET();
    delay_us(I2C_DELAY_US);
    SCL_CLR();
    SDA_IN();
    return data;
}

bool sw_i2c_write(uint8_t dev_addr, const uint8_t *buf, uint32_t len)
{
    i2c_start();
    if (i2c_write_byte((uint8_t)(dev_addr << 1))) { i2c_stop(); return false; }
    for (uint32_t i = 0; i < len; i++)
    {
        if (i2c_write_byte(buf[i])) { i2c_stop(); return false; }
    }
    i2c_stop();
    return true;
}

bool sw_i2c_read(uint8_t dev_addr, uint8_t *buf, uint32_t len)
{
    i2c_start();
    if (i2c_write_byte((uint8_t)(dev_addr << 1) | 1)) { i2c_stop(); return false; }
    for (uint32_t i = 0; i < len; i++)
        buf[i] = i2c_read_byte(i == (len - 1));
    i2c_stop();
    return true;
}

bool sw_i2c_write_read(uint8_t dev_addr, const uint8_t *wbuf, uint32_t wlen,
                       uint8_t *rbuf, uint32_t rlen)
{
    i2c_start();
    if (i2c_write_byte((uint8_t)(dev_addr << 1))) { i2c_stop(); return false; }
    for (uint32_t i = 0; i < wlen; i++)
    {
        if (i2c_write_byte(wbuf[i])) { i2c_stop(); return false; }
    }
    /* Repeated START */
    SDA_SET();
    delay_us(I2C_DELAY_US);
    SCL_SET();
    delay_us(I2C_DELAY_US);
    SDA_CLR();
    delay_us(I2C_DELAY_US);
    SCL_CLR();
    /* Send read address */
    if (i2c_write_byte((uint8_t)(dev_addr << 1) | 1)) { i2c_stop(); return false; }
    for (uint32_t i = 0; i < rlen; i++)
        rbuf[i] = i2c_read_byte(i == (rlen - 1));
    i2c_stop();
    return true;
}
