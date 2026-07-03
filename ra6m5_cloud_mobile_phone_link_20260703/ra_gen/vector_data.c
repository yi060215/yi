/* generated vector source file - do not edit */
#include "bsp_api.h"

#if VECTOR_DATA_IRQ_COUNT > 0
BSP_DONT_REMOVE const fsp_vector_t g_vector_table[BSP_ICU_VECTOR_NUM_ENTRIES] BSP_PLACE_IN_SECTION(BSP_SECTION_APPLICATION_VECTORS) =
{
    [0] = sci_uart_rxi_isr,
    [1] = sci_uart_txi_isr,
    [2] = sci_uart_tei_isr,
    [3] = sci_uart_eri_isr,
    [4] = sci_uart_rxi_isr,
    [5] = sci_uart_txi_isr,
    [6] = sci_uart_tei_isr,
    [7] = sci_uart_eri_isr,
};

const bsp_interrupt_event_t g_interrupt_event_link_select[BSP_ICU_VECTOR_NUM_ENTRIES] =
{
    [0] = ELC_EVENT_SCI2_RXI,
    [1] = ELC_EVENT_SCI2_TXI,
    [2] = ELC_EVENT_SCI2_TEI,
    [3] = ELC_EVENT_SCI2_ERI,
    [4] = ELC_EVENT_SCI7_RXI,
    [5] = ELC_EVENT_SCI7_TXI,
    [6] = ELC_EVENT_SCI7_TEI,
    [7] = ELC_EVENT_SCI7_ERI,
};
#endif
