-
/* generated HAL source file - do not edit */
#include <stdio.h>
#include "drv_esp32.h"
#include "hal_data.h"

sci_uart_instance_ctrl_t g_uart2_ctrl;
sci_uart_instance_ctrl_t g_uart7_ctrl;

static baud_setting_t g_uart2_baud_setting =
{
    .semr_baudrate_bits_b.abcse = 0,
    .semr_baudrate_bits_b.abcs = 0,
    .semr_baudrate_bits_b.bgdm = 1,
    .cks = 0,
    .brr = 53,
    .mddr = (uint8_t) 256,
    .semr_baudrate_bits_b.brme = false
};

static baud_setting_t g_uart7_baud_setting =
{
    .semr_baudrate_bits_b.abcse = 0,
    .semr_baudrate_bits_b.abcs = 0,
    .semr_baudrate_bits_b.bgdm = 1,
    .cks = 0,
    .brr = 53,
    .mddr = (uint8_t) 256,
    .semr_baudrate_bits_b.brme = false
};

const sci_uart_extended_cfg_t g_uart2_cfg_extend =
{
    .clock = SCI_UART_CLOCK_INT,
    .rx_edge_start = SCI_UART_START_BIT_FALLING_EDGE,
    .noise_cancel = SCI_UART_NOISE_CANCELLATION_DISABLE,
    .p_baud_setting = &g_uart2_baud_setting,
    .rx_fifo_trigger = SCI_UART_RX_FIFO_TRIGGER_1,
    .flow_control_pin = (bsp_io_port_pin_t) UINT16_MAX,
    .flow_control = SCI_UART_FLOW_CONTROL_RTS,
    .rs485_setting =
    {
        .enable = SCI_UART_RS485_DISABLE,
        .polarity = SCI_UART_RS485_DE_POLARITY_HIGH,
        .de_control_pin = (bsp_io_port_pin_t) UINT16_MAX,
    },
    .irda_setting =
    {
        .ircr_bits_b.ire = 0,
        .ircr_bits_b.irrxinv = 0,
        .ircr_bits_b.irtxinv = 0,
    },
};

const sci_uart_extended_cfg_t g_uart7_cfg_extend =
{
    .clock = SCI_UART_CLOCK_INT,
    .rx_edge_start = SCI_UART_START_BIT_FALLING_EDGE,
    .noise_cancel = SCI_UART_NOISE_CANCELLATION_DISABLE,
    .p_baud_setting = &g_uart7_baud_setting,
    .rx_fifo_trigger = SCI_UART_RX_FIFO_TRIGGER_1,
    .flow_control_pin = (bsp_io_port_pin_t) UINT16_MAX,
    .flow_control = SCI_UART_FLOW_CONTROL_RTS,
    .rs485_setting =
    {
        .enable = SCI_UART_RS485_DISABLE,
        .polarity = SCI_UART_RS485_DE_POLARITY_HIGH,
        .de_control_pin = (bsp_io_port_pin_t) UINT16_MAX,
    },
    .irda_setting =
    {
        .ircr_bits_b.ire = 0,
        .ircr_bits_b.irrxinv = 0,
        .ircr_bits_b.irtxinv = 0,
    },
};

const uart_cfg_t g_uart2_cfg =
{
    .channel = 2,
    .data_bits = UART_DATA_BITS_8,
    .parity = UART_PARITY_OFF,
    .stop_bits = UART_STOP_BITS_1,
    .p_callback = uart2_callback,
    .p_context = NULL,
    .p_extend = &g_uart2_cfg_extend,
    .p_transfer_tx = NULL,
    .p_transfer_rx = NULL,
    .rxi_ipl = (12),
    .txi_ipl = (12),
    .tei_ipl = (12),
    .eri_ipl = (12),
#if defined(VECTOR_NUMBER_SCI2_RXI)
    .rxi_irq = VECTOR_NUMBER_SCI2_RXI,
#else
    .rxi_irq = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_SCI2_TXI)
    .txi_irq = VECTOR_NUMBER_SCI2_TXI,
#else
    .txi_irq = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_SCI2_TEI)
    .tei_irq = VECTOR_NUMBER_SCI2_TEI,
#else
    .tei_irq = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_SCI2_ERI)
    .eri_irq = VECTOR_NUMBER_SCI2_ERI,
#else
    .eri_irq = FSP_INVALID_VECTOR,
#endif
};

const uart_instance_t g_uart2 =
{
    .p_ctrl = &g_uart2_ctrl,
    .p_cfg = &g_uart2_cfg,
    .p_api = &g_uart_on_sci
};

const uart_cfg_t g_uart7_cfg =
{
    .channel = 7,
    .data_bits = UART_DATA_BITS_8,
    .parity = UART_PARITY_OFF,
    .stop_bits = UART_STOP_BITS_1,
    .p_callback = uart7_callback,
    .p_context = NULL,
    .p_extend = &g_uart7_cfg_extend,
    .p_transfer_tx = NULL,
    .p_transfer_rx = NULL,
    .rxi_ipl = (12),
    .txi_ipl = (12),
    .tei_ipl = (12),
    .eri_ipl = (12),
#if defined(VECTOR_NUMBER_SCI7_RXI)
    .rxi_irq = VECTOR_NUMBER_SCI7_RXI,
#else
    .rxi_irq = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_SCI7_TXI)
    .txi_irq = VECTOR_NUMBER_SCI7_TXI,
#else
    .txi_irq = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_SCI7_TEI)
    .tei_irq = VECTOR_NUMBER_SCI7_TEI,
#else
    .tei_irq = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_SCI7_ERI)
    .eri_irq = VECTOR_NUMBER_SCI7_ERI,
#else
    .eri_irq = FSP_INVALID_VECTOR,
#endif
};

const uart_instance_t g_uart7 =
{
    .p_ctrl = &g_uart7_ctrl,
    .p_cfg = &g_uart7_cfg,
    .p_api = &g_uart_on_sci
};

void g_hal_init(void)
{
    g_common_init();
    g_uart2.p_api->open(g_uart2.p_ctrl, g_uart2.p_cfg);
    g_uart7.p_api->open(g_uart7.p_ctrl, g_uart7.p_cfg);
}

void uart2_callback(uart_callback_args_t * p_args)
{
    extern void uart_bridge_uart2_callback(uart_callback_args_t * p_args);
    uart_bridge_uart2_callback(p_args);
}

void uart7_callback(uart_callback_args_t * p_args)
{
    extern void uart_bridge_uart7_callback(uart_callback_args_t * p_args);
    uart_bridge_uart7_callback(p_args);
}
