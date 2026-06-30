#include "hal_data.h"
#include "drv_wifi.h"
#include "libs.h"
#include "config.h"
#include "errno.h"
#include <string.h>

FSP_CPP_HEADER
void R_BSP_WarmStart(bsp_warm_start_event_t event);
FSP_CPP_FOOTER

volatile uint32_t g_wifi_diag_state = 0;
volatile int32_t  g_wifi_diag_error = 0;
volatile char     g_wifi_diag_ssid[32] = WIFI_SSID;
volatile char     g_wifi_diag_ip[16] = {0};

static void led_set(bsp_io_level_t level)
{
    R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_04_PIN_00, level);
}

static void led_delay_ms(uint32_t ms)
{
    R_BSP_SoftwareDelay(ms, BSP_DELAY_UNITS_MILLISECONDS);
}

static void led_blink(uint32_t count, uint32_t interval_ms)
{
    for (uint32_t i = 0; i < count; i++) {
        led_set(BSP_IO_LEVEL_HIGH);
        led_delay_ms(interval_ms);
        led_set(BSP_IO_LEVEL_LOW);
        led_delay_ms(interval_ms);
    }
}

void hal_entry(void)
{
    g_wifi_diag_state = 1;
    g_wifi_diag_error = 0;
    memset((void *)g_wifi_diag_ip, 0, sizeof(g_wifi_diag_ip));
    led_blink(1, 200);

    g_wifi_diag_state = 2;
    if (wifi_init() != ESUCCESS) {
        g_wifi_diag_state = 101;
        g_wifi_diag_error = -1;
        while (1) {
            led_blink(1, 700);
            led_delay_ms(700);
        }
    }

    g_wifi_diag_state = 3;
    led_blink(3, 120);
    led_delay_ms(1000);

    g_wifi_diag_state = 4;
    while (wifi_connect(WIFI_SSID, WIFI_PASSWORD) != ESUCCESS) {
        uint32_t error_stage = (uint32_t)wifi_last_error_stage();
        if (error_stage == WIFI_ERR_STAGE_NONE) {
            error_stage = WIFI_ERR_STAGE_CONNECT_AP;
        }

        g_wifi_diag_state = 100 + error_stage;
        g_wifi_diag_error = (int32_t)error_stage;
        led_blink(error_stage, 500);
        led_delay_ms(3000);
    }

    g_wifi_diag_state = 5;
    g_wifi_diag_error = 0;
    wifi_get_ip((char *)g_wifi_diag_ip, (int)sizeof(g_wifi_diag_ip));

    while (1) {
        led_blink(5, 120);
        led_delay_ms(1000);
    }

#if BSP_TZ_SECURE_BUILD
    R_BSP_NonSecureEnter();
#endif
}

void R_BSP_WarmStart(bsp_warm_start_event_t event)
{
    if (BSP_WARM_START_RESET == event) {
#if BSP_FEATURE_FLASH_LP_VERSION != 0
        R_FACI_LP->DFLCTL = 1U;
#endif
    }

    if (BSP_WARM_START_POST_C == event) {
        R_IOPORT_Open(&g_ioport_ctrl, g_ioport.p_cfg);
    }
}

#if BSP_TZ_SECURE_BUILD
BSP_CMSE_NONSECURE_ENTRY void template_nonsecure_callable(void);

BSP_CMSE_NONSECURE_ENTRY void template_nonsecure_callable(void)
{
}
#endif
