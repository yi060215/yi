/* generated vector header file - do not edit */
#ifndef VECTOR_DATA_H
#define VECTOR_DATA_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef VECTOR_DATA_IRQ_COUNT
#define VECTOR_DATA_IRQ_COUNT    (8)
#endif

void sci_uart_rxi_isr(void);
void sci_uart_txi_isr(void);
void sci_uart_tei_isr(void);
void sci_uart_eri_isr(void);

#define VECTOR_NUMBER_SCI2_RXI ((IRQn_Type) 0)
#define SCI2_RXI_IRQn          ((IRQn_Type) 0)
#define VECTOR_NUMBER_SCI2_TXI ((IRQn_Type) 1)
#define SCI2_TXI_IRQn          ((IRQn_Type) 1)
#define VECTOR_NUMBER_SCI2_TEI ((IRQn_Type) 2)
#define SCI2_TEI_IRQn          ((IRQn_Type) 2)
#define VECTOR_NUMBER_SCI2_ERI ((IRQn_Type) 3)
#define SCI2_ERI_IRQn          ((IRQn_Type) 3)
#define VECTOR_NUMBER_SCI7_RXI ((IRQn_Type) 4)
#define SCI7_RXI_IRQn          ((IRQn_Type) 4)
#define VECTOR_NUMBER_SCI7_TXI ((IRQn_Type) 5)
#define SCI7_TXI_IRQn          ((IRQn_Type) 5)
#define VECTOR_NUMBER_SCI7_TEI ((IRQn_Type) 6)
#define SCI7_TEI_IRQn          ((IRQn_Type) 6)
#define VECTOR_NUMBER_SCI7_ERI ((IRQn_Type) 7)
#define SCI7_ERI_IRQn          ((IRQn_Type) 7)

#define BSP_ICU_VECTOR_NUM_ENTRIES (8)

#ifdef __cplusplus
}
#endif
#endif /* VECTOR_DATA_H */
