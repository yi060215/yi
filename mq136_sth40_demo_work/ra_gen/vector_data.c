/* generated vector source file - do not edit */
        #include "bsp_api.h"
        /* Do not build these data structures if no interrupts are currently allocated because IAR will have build errors. */
        #if VECTOR_DATA_IRQ_COUNT > 0
        BSP_DONT_REMOVE const fsp_vector_t g_vector_table[BSP_ICU_VECTOR_NUM_ENTRIES] BSP_PLACE_IN_SECTION(BSP_SECTION_APPLICATION_VECTORS) =
        {
                        [0] = sci_uart_rxi_isr, /* SCI7 RXI (Receive data full) */
            [1] = sci_uart_txi_isr, /* SCI7 TXI (Transmit data empty) */
            [2] = sci_uart_tei_isr, /* SCI7 TEI (Transmit end) */
            [3] = sci_uart_eri_isr, /* SCI7 ERI (Receive error) */
            [4] = spi_rxi_isr, /* SPI0 RXI (Receive buffer full) */
            [5] = spi_txi_isr, /* SPI0 TXI (Transmit buffer empty) */
            [6] = spi_tei_isr, /* SPI0 TEI (Transmission complete event) */
            [7] = spi_eri_isr, /* SPI0 ERI (Error) */
            [8] = iic_master_rxi_isr, /* IIC2 RXI (Receive data full) */
            [9] = iic_master_txi_isr, /* IIC2 TXI (Transmit data empty) */
            [10] = iic_master_tei_isr, /* IIC2 TEI (Transmit end) */
            [11] = iic_master_eri_isr, /* IIC2 ERI (Transfer error) */
        };
        #if BSP_FEATURE_ICU_HAS_IELSR
        const bsp_interrupt_event_t g_interrupt_event_link_select[BSP_ICU_VECTOR_NUM_ENTRIES] =
        {
            [0] = BSP_PRV_VECT_ENUM(EVENT_SCI7_RXI,GROUP0), /* SCI7 RXI (Receive data full) */
            [1] = BSP_PRV_VECT_ENUM(EVENT_SCI7_TXI,GROUP1), /* SCI7 TXI (Transmit data empty) */
            [2] = BSP_PRV_VECT_ENUM(EVENT_SCI7_TEI,GROUP2), /* SCI7 TEI (Transmit end) */
            [3] = BSP_PRV_VECT_ENUM(EVENT_SCI7_ERI,GROUP3), /* SCI7 ERI (Receive error) */
            [4] = BSP_PRV_VECT_ENUM(EVENT_SPI0_RXI,GROUP4), /* SPI0 RXI (Receive buffer full) */
            [5] = BSP_PRV_VECT_ENUM(EVENT_SPI0_TXI,GROUP5), /* SPI0 TXI (Transmit buffer empty) */
            [6] = BSP_PRV_VECT_ENUM(EVENT_SPI0_TEI,GROUP6), /* SPI0 TEI (Transmission complete event) */
            [7] = BSP_PRV_VECT_ENUM(EVENT_SPI0_ERI,GROUP7), /* SPI0 ERI (Error) */
            [8] = BSP_PRV_VECT_ENUM(EVENT_IIC2_RXI,GROUP0), /* IIC2 RXI (Receive data full) */
            [9] = BSP_PRV_VECT_ENUM(EVENT_IIC2_TXI,GROUP1), /* IIC2 TXI (Transmit data empty) */
            [10] = BSP_PRV_VECT_ENUM(EVENT_IIC2_TEI,GROUP2), /* IIC2 TEI (Transmit end) */
            [11] = BSP_PRV_VECT_ENUM(EVENT_IIC2_ERI,GROUP3), /* IIC2 ERI (Transfer error) */
        };
        #endif
        #endif