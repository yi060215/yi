/*
 * drv_sci_spi.c
 *
 *  Created on: 2023年3月30日
 *      Author: slhuan
 */

#include "drv_spi.h"
#include <devices.h>
#include <errno.h>
#include <libs.h>
#include "hal_data.h"

static int SPIDrvInit(struct SPIDev *ptdev);
static int SPIDrvWrite(struct SPIDev *ptdev, const unsigned char *buf, unsigned int length);
static int SPIDrvRead(struct SPIDev *ptdev, unsigned char *buf, unsigned int length);
static int SPIDrvWriteRead(struct SPIDev *ptdev, unsigned char * const wbuf, unsigned char *rbuf, unsigned int length);
static void SPI0DrvWaitTxCplt(void);

static volatile bool gSPI0TxCplt = false;
static IODevice  *pCSIO   = NULL;
static struct SPIDev gSPIDevice = {
    .name       = "ADXL345 SPI",
    .channel    = 0,
    .Init       = SPIDrvInit,
    .Read       = SPIDrvRead,
    .Write      = SPIDrvWrite,
    .WriteRead  = SPIDrvWriteRead,
    .next = NULL
};

void SPIDevicesCreate(void)
{
    SPIDeviceInsert(&gSPIDevice);
}

static int SPIDrvInit(struct SPIDev *ptdev)
{
    if(NULL == ptdev)   return -EINVAL;
    switch(ptdev->channel)
    {
        case 0:
        {
            pCSIO = IODeviceFind("ADXL345 CS");
            if(NULL == pCSIO)      return -ENXIO;
            /* 打开设备 */
            fsp_err_t err = g_spi0.p_api->open(g_spi0.p_ctrl, g_spi0.p_cfg);
            assert(FSP_SUCCESS == err);
            break;
        }
        case 1:case 2:case 3:case 4:
        case 5:
        case 6:case 7:case 8:case 9:
            break;
        default:break;
    }
    return ESUCCESS;
}

static int SPIDrvWrite(struct SPIDev *ptdev, const unsigned char *buf, unsigned int length)
{
    if(NULL == ptdev)   return -EINVAL;
    if(NULL == buf)     return -EINVAL;
    if(0 == length)     return -EINVAL;
    
    switch(ptdev->channel)
    {
        case 0:
        {
            pCSIO->Write(pCSIO, 0);
            fsp_err_t err = FSP_SUCCESS;
            if((length%4)==0)
            {
                length = length>>2;
                err = g_spi0.p_api->write(g_spi0.p_ctrl, buf, length, SPI_BIT_WIDTH_32_BITS);
            }
            else if((length%2)==0)
            {
                length = length>>1;
                err = g_spi0.p_api->write(g_spi0.p_ctrl, buf, length, SPI_BIT_WIDTH_16_BITS);
            }
            else if(length==1)
            {
                err = g_spi0.p_api->write(g_spi0.p_ctrl, buf, length, SPI_BIT_WIDTH_8_BITS);
            }
            assert(FSP_SUCCESS == err);
            SPI0DrvWaitTxCplt();
            pCSIO->Write(pCSIO, 1);
            break;
        }
        case 1:case 2:case 3:case 4:
            break;
        case 5:
        case 6:case 7:case 8:case 9:
            break;
        default:break;
    }
    return ESUCCESS;
}

static int SPIDrvRead(struct SPIDev *ptdev, unsigned char *buf, unsigned int length)
{
    if(NULL == ptdev)   return -EINVAL;
    if(NULL == buf)     return -EINVAL;
    if(0 == length)     return -EINVAL;
    
    switch(ptdev->channel)
    {
        case 0:
        {
            pCSIO->Write(pCSIO, 0);
            fsp_err_t err = FSP_SUCCESS;
            if((length%4)==0)
            {
                length = length>>2;
                err = g_spi0.p_api->read(g_spi0.p_ctrl, buf, length, SPI_BIT_WIDTH_32_BITS);
            }
            else if((length%2)==0)
            {
                length = length>>1;
                err = g_spi0.p_api->read(g_spi0.p_ctrl, buf, length, SPI_BIT_WIDTH_16_BITS);
            }
            else if(length==1)
            {
                err = g_spi0.p_api->read(g_spi0.p_ctrl, buf, length, SPI_BIT_WIDTH_8_BITS);
            }
            assert(FSP_SUCCESS == err);
            SPI0DrvWaitTxCplt();
            pCSIO->Write(pCSIO, 1);
            break;
        }
        case 1:case 2:case 3:case 4:
            break;
        case 5:
        case 6:case 7:case 8:case 9:
            break;
        default:break;
    }
    return ESUCCESS;
}

static int SPIDrvWriteRead(struct SPIDev *ptdev, unsigned char * const wbuf, unsigned char *rbuf, unsigned int length)
{
    if(NULL == ptdev)   return -EINVAL;
    if(NULL == wbuf)    return -EINVAL;
    if(NULL == rbuf)    return -EINVAL;
    if(0 == length)     return -EINVAL;
    
    switch(ptdev->channel)
    {
        case 0:
        {
            pCSIO->Write(pCSIO, 0);
            fsp_err_t err = g_spi0.p_api->writeRead(g_spi0.p_ctrl, wbuf, rbuf, length, SPI_BIT_WIDTH_8_BITS);
            assert(FSP_SUCCESS == err);
            SPI0DrvWaitTxCplt();
            pCSIO->Write(pCSIO, 1);
            break;
        }
        case 1:case 2:case 3:case 4:
            break;
        case 5:
        case 6:case 7:case 8:case 9:
            break;
        default:break;
    }
    return ESUCCESS;
}

void spi0_callback(spi_callback_args_t *p_args)
{
    switch(p_args->event)
    {
        case SPI_EVENT_TRANSFER_COMPLETE:
        {
            gSPI0TxCplt = true;
            break;
        }
        case SPI_EVENT_TRANSFER_ABORTED:
        case SPI_EVENT_ERR_MODE_FAULT:  
        case SPI_EVENT_ERR_READ_OVERFLOW:
        case SPI_EVENT_ERR_PARITY:
        case SPI_EVENT_ERR_OVERRUN:
        case SPI_EVENT_ERR_FRAMING:
        case SPI_EVENT_ERR_MODE_UNDERRUN:
        {
            break;
        }
        default:break;
    }
}

static void SPI0DrvWaitTxCplt(void)
{
    while(!gSPI0TxCplt);
    gSPI0TxCplt = false;
}
