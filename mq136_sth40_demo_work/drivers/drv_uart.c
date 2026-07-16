/*
 * drv_uart.c
 *
 *  Created on: 2023年3月30日
 *      Author: slhuan
 */

#include "drv_uart.h"
#include <devices.h>
#include "hal_data.h"
#include <errno.h>

static void UART7WaitTxCplt(void);
static void UART7WaitRxCplt(void);

static int UARTDrvInit(struct UartDev *ptdev);
static int UARTDrvRead(struct UartDev *ptdev, unsigned char *buf, unsigned int length);
static int UARTDrvWrite(struct UartDev *ptdev, unsigned char * const buf, unsigned int length);

static volatile bool gUart7TxCplt = false;
static volatile bool gUart7RxCplt = false;

static struct UartDev gLogDevice = {
    .name = "Log",
    .channel = 7,
    .Init = UARTDrvInit,
    .Read = UARTDrvRead,
    .Write = UARTDrvWrite,
    .next = NULL
};

void UartDevicesCreate(void)
{
    UartDeviceInsert(&gLogDevice);
    gLogDevice.Init(&gLogDevice);
}

static int UARTDrvInit(struct UartDev *ptdev)
{
    if(NULL == ptdev)   return -EINVAL;
    
    switch(ptdev->channel)
    {
        case 0:case 1:case 2:
        case 3:case 4:case 5:
        case 6:
        {
            break;
        }
        case 7:
        {
            fsp_err_t err = g_uart7.p_api->open(g_uart7.p_ctrl, g_uart7.p_cfg);
            assert(FSP_SUCCESS == err);
            break;
        }
        case 8:case 9:
            break;
        default:break;
    }
    
    return ESUCCESS;
}

static int UARTDrvWrite(struct UartDev *ptdev, unsigned char * const buf, unsigned int length)
{
    if(NULL == ptdev)   return -EINVAL;
    if(NULL == buf)     return -EINVAL;
    if(0 == length)     return -EINVAL;
    
    switch(ptdev->channel)
    {
        case 0:case 1:case 2:
        case 3:case 4:case 5:
        case 6:
        {
            break;
        }
        case 7:
        {
            fsp_err_t err = g_uart7.p_api->write(g_uart7.p_ctrl, buf, length);
            assert(FSP_SUCCESS == err);
            UART7WaitTxCplt();
            break;
        }
        case 8:case 9:
            break;
        default:break;
    }
    return ESUCCESS;
}
static int UARTDrvRead(struct UartDev *ptdev, unsigned char *buf, unsigned int length)
{
    if(NULL == ptdev)   return -EINVAL;
    if(NULL == buf)     return -EINVAL;
    if(0 == length)     return -EINVAL;

    switch(ptdev->channel)
    {
        case 0:case 1:case 2:
        case 3:case 4:case 5:
        case 6:
        {
            break;
        }
        case 7:
        {
            fsp_err_t err = g_uart7.p_api->read(g_uart7.p_ctrl, buf, length);
            assert(FSP_SUCCESS == err);
            UART7WaitRxCplt();
            break;
        }
        case 8:case 9:
            break;
        default:break;
    }
    
    return (int)length;
}

void uart7_callback(uart_callback_args_t * p_args)
{
    switch(p_args->event)
    {
        case UART_EVENT_RX_COMPLETE:
        {
            gUart7RxCplt = true;
            break;
        }
        case UART_EVENT_TX_COMPLETE:
        {
            gUart7TxCplt = true;
            break;
        }
        case UART_EVENT_RX_CHAR:
        {
            break;
        }
        case UART_EVENT_ERR_PARITY:case UART_EVENT_ERR_FRAMING:
        case UART_EVENT_ERR_OVERFLOW:case UART_EVENT_BREAK_DETECT:
        case UART_EVENT_TX_DATA_EMPTY:
            break;
        default:break;   
    }
}

static void UART7WaitTxCplt(void)
{
    while(!gUart7TxCplt);
    gUart7TxCplt = false;
}

static void UART7WaitRxCplt(void)
{
    while(!gUart7RxCplt);
    gUart7RxCplt = false;
}
