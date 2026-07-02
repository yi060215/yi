#include "hal_data.h"
#include "config.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

FSP_CPP_HEADER
void R_BSP_WarmStart(bsp_warm_start_event_t event);
FSP_CPP_FOOTER

#define BRIDGE_BUF_SIZE      1024U
#define AT_RESP_SIZE         2048U
#define HTTP_REQ_SIZE        1800U
#define JSON_BODY_SIZE       900U

static volatile uint8_t  g_pc_to_esp_buf[BRIDGE_BUF_SIZE];
static volatile uint16_t g_pc_to_esp_head;
static volatile uint16_t g_pc_to_esp_tail;
static volatile uint8_t  g_esp_to_pc_buf[BRIDGE_BUF_SIZE];
static volatile uint16_t g_esp_to_pc_head;
static volatile uint16_t g_esp_to_pc_tail;

static volatile bool g_pc_tx_busy;
static volatile bool g_esp_tx_busy;
static uint8_t       g_pc_rx_byte;
static uint8_t       g_esp_rx_byte;
static uint8_t       g_pc_tx_byte;
static uint8_t       g_esp_tx_byte;

static char g_at_resp[AT_RESP_SIZE];
static char g_http_req[HTTP_REQ_SIZE];
static char g_pending_json[JSON_BODY_SIZE];
static char g_http_resp_copy[AT_RESP_SIZE];

volatile uint32_t g_direct_upload_state;
volatile int32_t  g_direct_upload_error;

static void led_set(bsp_io_level_t level)
{
    R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_04_PIN_00, level);
}

static void delay_ms(uint32_t ms)
{
    R_BSP_SoftwareDelay(ms, BSP_DELAY_UNITS_MILLISECONDS);
}

static uint16_t ring_next(uint16_t value)
{
    value++;
    return (value >= BRIDGE_BUF_SIZE) ? 0U : value;
}

static void ring_put(volatile uint8_t * buf, volatile uint16_t * head, volatile uint16_t * tail, uint8_t data)
{
    uint16_t next = ring_next(*head);
    if (next == *tail) {
        return;
    }
    buf[*head] = data;
    *head = next;
}

static bool ring_get(volatile uint8_t * buf, volatile uint16_t * head, volatile uint16_t * tail, uint8_t * data)
{
    if (*tail == *head) {
        return false;
    }
    *data = buf[*tail];
    *tail = ring_next(*tail);
    return true;
}

static void esp_ring_clear(void)
{
    g_esp_to_pc_head = 0;
    g_esp_to_pc_tail = 0;
}

static void uart_write_byte(const uart_instance_t * uart, volatile bool * busy, uint8_t * tx_byte, uint8_t data)
{
    uint32_t timeout = 200U;
    while (*busy && (timeout > 0U)) {
        delay_ms(1U);
        timeout--;
    }

    *tx_byte = data;
    *busy = true;
    if (uart->p_api->write(uart->p_ctrl, tx_byte, 1U) != FSP_SUCCESS) {
        *busy = false;
        return;
    }

    timeout = 200U;
    while (*busy && (timeout > 0U)) {
        delay_ms(1U);
        timeout--;
    }
    if (timeout == 0U) {
        *busy = false;
    }
}

static void pc_write_text(const char * text)
{
    while ((text != NULL) && (*text != '\0')) {
        uart_write_byte(&g_uart7, &g_pc_tx_busy, &g_pc_tx_byte, (uint8_t) *text);
        text++;
    }
}

static void esp_write_text(const char * text)
{
    while ((text != NULL) && (*text != '\0')) {
        uart_write_byte(&g_uart2, &g_esp_tx_busy, &g_esp_tx_byte, (uint8_t) *text);
        text++;
    }
}

static void pc_printf(const char * fmt, ...)
{
    char buf[192];
    va_list args;
    int len;

    va_start(args, fmt);
    len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len > 0) {
        buf[sizeof(buf) - 1U] = '\0';
        pc_write_text(buf);
    }
}

static void uart_start_pc_read(void)
{
    (void) g_uart7.p_api->read(g_uart7.p_ctrl, &g_pc_rx_byte, 1U);
}

static void uart_start_esp_read(void)
{
    (void) g_uart2.p_api->read(g_uart2.p_ctrl, &g_esp_rx_byte, 1U);
}

void uart_bridge_uart2_callback(uart_callback_args_t * p_args)
{
    switch (p_args->event) {
    case UART_EVENT_RX_COMPLETE:
        ring_put(g_esp_to_pc_buf, &g_esp_to_pc_head, &g_esp_to_pc_tail, g_esp_rx_byte);
        uart_start_esp_read();
        break;
    case UART_EVENT_RX_CHAR:
        ring_put(g_esp_to_pc_buf, &g_esp_to_pc_head, &g_esp_to_pc_tail, (uint8_t) p_args->data);
        break;
    case UART_EVENT_TX_COMPLETE:
    case UART_EVENT_TX_DATA_EMPTY:
        g_esp_tx_busy = false;
        break;
    case UART_EVENT_ERR_PARITY:
    case UART_EVENT_ERR_FRAMING:
    case UART_EVENT_ERR_OVERFLOW:
        uart_start_esp_read();
        break;
    default:
        break;
    }
}

void uart_bridge_uart7_callback(uart_callback_args_t * p_args)
{
    switch (p_args->event) {
    case UART_EVENT_RX_COMPLETE:
        ring_put(g_pc_to_esp_buf, &g_pc_to_esp_head, &g_pc_to_esp_tail, g_pc_rx_byte);
        uart_start_pc_read();
        break;
    case UART_EVENT_RX_CHAR:
        ring_put(g_pc_to_esp_buf, &g_pc_to_esp_head, &g_pc_to_esp_tail, (uint8_t) p_args->data);
        break;
    case UART_EVENT_TX_COMPLETE:
    case UART_EVENT_TX_DATA_EMPTY:
        g_pc_tx_busy = false;
        break;
    default:
        break;
    }
}

static bool wait_resp(const char * ok1, const char * ok2, const char * fail, uint32_t timeout_ms)
{
    uint32_t elapsed = 0;
    uint32_t pos = 0;
    uint8_t ch;

    memset(g_at_resp, 0, sizeof(g_at_resp));
    while (elapsed < timeout_ms) {
        while (ring_get(g_esp_to_pc_buf, &g_esp_to_pc_head, &g_esp_to_pc_tail, &ch)) {
            if (pos < (sizeof(g_at_resp) - 1U)) {
                g_at_resp[pos++] = (char) ch;
                g_at_resp[pos] = '\0';
            }
            uart_write_byte(&g_uart7, &g_pc_tx_busy, &g_pc_tx_byte, ch);
            if ((ok1 != NULL) && (strstr(g_at_resp, ok1) != NULL)) {
                return true;
            }
            if ((ok2 != NULL) && (strstr(g_at_resp, ok2) != NULL)) {
                return true;
            }
            if ((fail != NULL) && (strstr(g_at_resp, fail) != NULL)) {
                return false;
            }
        }
        delay_ms(10U);
        elapsed += 10U;
    }

    return false;
}

static bool at_cmd(const char * label, const char * cmd, const char * ok1, const char * ok2, uint32_t timeout_ms)
{
    pc_printf("\r\n[AT] %s\r\n", label);
    esp_ring_clear();
    esp_write_text(cmd);
    if (wait_resp(ok1, ok2, "ERROR", timeout_ms)) {
        pc_write_text("\r\n[AT] OK\r\n");
        return true;
    }
    pc_write_text("\r\n[AT] FAIL/TIMEOUT\r\n");
    return false;
}

static char * http_body_ptr(char * resp)
{
    char * body;

    if (resp == NULL) {
        return NULL;
    }

    body = strstr(resp, "\r\n\r\n");
    if (body != NULL) {
        return body + 4;
    }

    body = strstr(resp, "\n\n");
    if (body != NULL) {
        return body + 2;
    }

    return resp;
}

static bool http_request_once(const char * method, const char * path, const char * body, char * resp_out, uint32_t resp_len)
{
    char cmd[96];
    int req_len;
    unsigned int body_len = (body != NULL) ? (unsigned int) strlen(body) : 0U;

    if ((method == NULL) || (path == NULL) || (resp_out == NULL) || (resp_len == 0U)) {
        return false;
    }

    if (body_len > 0U) {
        snprintf(g_http_req,
                 sizeof(g_http_req),
                 "%s %s HTTP/1.1\r\n"
                 "Host: %s:%d\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: %u\r\n"
                 "Connection: close\r\n"
                 "\r\n"
                 "%s",
                 method,
                 path,
                 CLOUD_SERVER_HOST,
                 CLOUD_SERVER_PORT,
                 body_len,
                 body);
    } else {
        snprintf(g_http_req,
                 sizeof(g_http_req),
                 "%s %s HTTP/1.1\r\n"
                 "Host: %s:%d\r\n"
                 "Connection: close\r\n"
                 "\r\n",
                 method,
                 path,
                 CLOUD_SERVER_HOST,
                 CLOUD_SERVER_PORT);
    }

    req_len = (int) strlen(g_http_req);
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%d\r\n", CLOUD_SERVER_HOST, CLOUD_SERVER_PORT);
    if (!at_cmd("TCP connect", cmd, "OK", "ALREADY CONNECTED", 10000U)) {
        return false;
    }

    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d\r\n", req_len);
    if (!at_cmd("CIPSEND", cmd, ">", NULL, 5000U)) {
        return false;
    }

    pc_printf("\r\n[HTTP] %s %s sending %d bytes\r\n", method, path, req_len);
    esp_ring_clear();
    esp_write_text(g_http_req);
    if (!wait_resp("SEND OK", NULL, "SEND FAIL", 15000U)) {
        return false;
    }

    if (!wait_resp("CLOSED", NULL, NULL, 15000U)) {
        return false;
    }

    strncpy(resp_out, g_at_resp, resp_len - 1U);
    resp_out[resp_len - 1U] = '\0';
    return strstr(resp_out, "HTTP/1.1 200") != NULL || strstr(resp_out, "HTTP/1.0 200") != NULL;
}

static bool fetch_pending_record(void)
{
    char * body;

    memset(g_http_resp_copy, 0, sizeof(g_http_resp_copy));
    memset(g_pending_json, 0, sizeof(g_pending_json));

    if (!http_request_once("GET", "/api/test-record", NULL, g_http_resp_copy, sizeof(g_http_resp_copy))) {
        return false;
    }

    body = strstr(g_http_resp_copy, "{\"cmd\":\"upload_record\"");
    if (body == NULL) {
        body = http_body_ptr(g_http_resp_copy);
    }
    if ((body == NULL) || (strstr(body, "\"cmd\":\"upload_record\"") == NULL)) {
        pc_write_text("\r\n[FETCH] invalid pending JSON\r\n");
        return false;
    }

    strncpy(g_pending_json, body, sizeof(g_pending_json) - 1U);
    g_pending_json[sizeof(g_pending_json) - 1U] = '\0';

    char * closed = strstr(g_pending_json, "\r\nCLOSED");
    if (closed != NULL) {
        *closed = '\0';
    }
    closed = strstr(g_pending_json, "\nCLOSED");
    if (closed != NULL) {
        *closed = '\0';
    }

    pc_printf("\r\n[FETCH] pending len=%u\r\n", (unsigned int) strlen(g_pending_json));
    return true;
}

static bool upload_pending_record(void)
{
    memset(g_http_resp_copy, 0, sizeof(g_http_resp_copy));
    if (!http_request_once("POST", "/api/upload-record", g_pending_json, g_http_resp_copy, sizeof(g_http_resp_copy))) {
        return false;
    }

    return strstr(g_http_resp_copy, "upload_record_result") != NULL || strstr(g_http_resp_copy, "HTTP/1.1 200") != NULL;
}

void hal_entry(void)
{
    baud_setting_t baud_setting = {0};

    g_direct_upload_state = 1;
    g_direct_upload_error = 0;
    led_set(BSP_IO_LEVEL_LOW);

    if (R_SCI_UART_BaudCalculate(115200U, false, 5000U, &baud_setting) == FSP_SUCCESS) {
        (void) g_uart2.p_api->baudSet(g_uart2.p_ctrl, &baud_setting);
        (void) g_uart7.p_api->baudSet(g_uart7.p_ctrl, &baud_setting);
    }

    uart_start_pc_read();
    uart_start_esp_read();
    pc_write_text("\r\n[RA6M5] fetch pending record and upload firmware\r\n");
    pc_printf("[CFG] ssid=%s cloud=%s:%d\r\n", WIFI_SSID, CLOUD_SERVER_HOST, CLOUD_SERVER_PORT);
    delay_ms(1500U);

    while (!at_cmd("AT", "AT\r\n", "OK", NULL, 3000U)) {
        g_direct_upload_state = 101;
        delay_ms(1000U);
    }
    (void) at_cmd("ATE0", "ATE0\r\n", "OK", NULL, 3000U);
    (void) at_cmd("CWMODE", "AT+CWMODE=1\r\n", "OK", NULL, 3000U);
    (void) at_cmd("DHCP", "AT+CWDHCP=1,1\r\n", "OK", NULL, 3000U);

    while (1) {
        char join_cmd[160];
        snprintf(join_cmd, sizeof(join_cmd), "AT+CWJAP=\"%s\",\"%s\"\r\n", WIFI_SSID, WIFI_PASSWORD);
        g_direct_upload_state = 2;
        if (at_cmd("Join WiFi", join_cmd, "OK", "WIFI GOT IP", 30000U)) {
            break;
        }
        g_direct_upload_error = 2;
        delay_ms(2000U);
    }

    (void) at_cmd("IP", "AT+CIPSTA?\r\n", "OK", NULL, 3000U);
    (void) at_cmd("MUX", "AT+CIPMUX=0\r\n", "OK", NULL, 3000U);

    while (1) {
        g_direct_upload_state = 3;
        if (!fetch_pending_record()) {
            g_direct_upload_state = 302;
            g_direct_upload_error = 302;
            pc_write_text("\r\n[RESULT] fetch retry\r\n");
            (void) at_cmd("CIPCLOSE", "AT+CIPCLOSE\r\n", "OK", "CLOSED", 2000U);
            delay_ms(3000U);
            continue;
        }

        g_direct_upload_state = 4;
        if (upload_pending_record()) {
            g_direct_upload_state = 8;
            g_direct_upload_error = 0;
            pc_write_text("\r\n[RESULT] fetched record upload ok, hold success state\r\n");
            led_set(BSP_IO_LEVEL_HIGH);
            while (1) {
                delay_ms(1000U);
            }
        }

        g_direct_upload_state = 301;
        g_direct_upload_error = 301;
        pc_write_text("\r\n[RESULT] upload retry\r\n");
        led_set(BSP_IO_LEVEL_LOW);
        (void) at_cmd("CIPCLOSE", "AT+CIPCLOSE\r\n", "OK", "CLOSED", 2000U);
        delay_ms(3000U);
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


