#include "drv_esp32.h"
#include "libs.h"
#include <hal_data.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ESP32_UART_CHANNEL  2
#define ESP32_RX_BUF_SIZE   2048
#define ESP32_RING_BUF_SIZE 4096

#define TIMEOUT_AT_BASIC    1000
#define TIMEOUT_AT_CONNECT  30000
#define TIMEOUT_AT_TCP      10000
#define TIMEOUT_AT_SEND     5000

#define ESP32_ERR_AP_NOT_FOUND      201
#define ESP32_ERR_AUTH_FAIL         202
#define ESP32_ERR_CONNECT_TIMEOUT   203
#define ESP32_ERR_CONNECT_FAIL      204

extern const uart_instance_t g_uart2;

static uint8_t gEsp32ReadKick = 0;
static uint8_t gEsp32RxBuf[ESP32_RX_BUF_SIZE];
static volatile uint16_t gEsp32RingHead = 0;
static volatile uint16_t gEsp32RingTail = 0;
static uint8_t gEsp32RingBuf[ESP32_RING_BUF_SIZE];

static int esp32_send_cmd(const char *cmd);
static int esp32_send_data(const uint8_t *data, int len);
static int esp32_read_bytes(uint8_t *buf, int len, int timeout_ms);
static int esp32_read_byte(uint8_t *ch, int timeout_ms);
static void esp32_ring_write(uint8_t data);
static int esp32_ring_read(uint8_t *data);
static void esp32_ring_clear(void);
static void esp32_start_background_rx(void);
static int esp32_wait_response(int timeout_ms);
static int esp32_wait_wifi_join_response(int timeout_ms);
static int esp32_wait_scan_response(int timeout_ms);
static int esp32_wait_pattern(const char *pattern, int timeout_ms);
static void esp32_flush_rx(void);
static int esp32_response_ok(void);
static void esp32_delay_ms(int ms);
static int esp32_buffer_contains(const char *pattern);
static int esp32_scan_result_contains_ssid(const char *ssid);

void esp32_uart_callback(uart_callback_args_t *p_args)
{
    switch (p_args->event) {
    case UART_EVENT_RX_COMPLETE:
        esp32_ring_write(gEsp32ReadKick);
        esp32_start_background_rx();
        break;
    case UART_EVENT_TX_COMPLETE:
        break;
    case UART_EVENT_RX_CHAR:
        esp32_ring_write((uint8_t)p_args->data);
        break;
    case UART_EVENT_TX_DATA_EMPTY:
    case UART_EVENT_ERR_PARITY:
    case UART_EVENT_ERR_FRAMING:
    case UART_EVENT_ERR_OVERFLOW:
    case UART_EVENT_BREAK_DETECT:
    default:
        break;
    }
}

static int esp32_send_cmd(const char *cmd)
{
    if (!cmd) return -EINVAL;
    return (g_uart2.p_api->write(g_uart2.p_ctrl, (uint8_t *)cmd, (uint32_t)strlen(cmd)) == FSP_SUCCESS) ? ESUCCESS : -EIO;
}

static int esp32_send_data(const uint8_t *data, int len)
{
    if (!data || len <= 0) return -EINVAL;
    return (g_uart2.p_api->write(g_uart2.p_ctrl, (uint8_t *)data, (uint32_t)len) == FSP_SUCCESS) ? len : -EIO;
}

static void esp32_ring_write(uint8_t data)
{
    uint16_t next = (uint16_t)((gEsp32RingHead + 1U) % ESP32_RING_BUF_SIZE);
    if (next != gEsp32RingTail) {
        gEsp32RingBuf[gEsp32RingHead] = data;
        gEsp32RingHead = next;
    }
}

static int esp32_ring_read(uint8_t *data)
{
    if (!data) return -EINVAL;
    if (gEsp32RingHead == gEsp32RingTail) return -ETIMEDOUT;

    *data = gEsp32RingBuf[gEsp32RingTail];
    gEsp32RingTail = (uint16_t)((gEsp32RingTail + 1U) % ESP32_RING_BUF_SIZE);
    return ESUCCESS;
}

static void esp32_ring_clear(void)
{
    gEsp32RingHead = 0;
    gEsp32RingTail = 0;
}

static void esp32_start_background_rx(void)
{
    fsp_err_t err = g_uart2.p_api->read(g_uart2.p_ctrl, &gEsp32ReadKick, 1U);
    (void) err;
}

static int esp32_read_bytes(uint8_t *buf, int len, int timeout_ms)
{
    if (!buf || len <= 0) return -EINVAL;

    int elapsed = 0;
    int total = 0;

    while (total < len) {
        if (esp32_ring_read(&buf[total]) == ESUCCESS) {
            total++;
            elapsed = 0;
            continue;
        }

        esp32_delay_ms(10);
        elapsed += 10;
        if (elapsed >= timeout_ms) {
            return (total > 0) ? total : -ETIMEDOUT;
        }
    }

    return total;
}

static int esp32_read_byte(uint8_t *ch, int timeout_ms)
{
    return esp32_read_bytes(ch, 1, timeout_ms);
}

static void esp32_flush_rx(void)
{
    esp32_ring_clear();
}

static void esp32_delay_ms(int ms)
{
    for (int i = 0; i < ms; ++i) {
        R_BSP_SoftwareDelay(1000, BSP_DELAY_UNITS_MICROSECONDS);
    }
}

static int esp32_buffer_contains(const char *pattern)
{
    return strstr((const char *)gEsp32RxBuf, pattern) != NULL;
}

static int esp32_scan_result_contains_ssid(const char *ssid)
{
    char quoted_ssid[96];
    snprintf(quoted_ssid, sizeof(quoted_ssid), "\"%s\"", ssid);

    const char *entry = strstr((const char *)gEsp32RxBuf, "+CWLAP:");
    while (entry) {
        const char *line_end = strstr(entry, "\r\n");
        const char *match = strstr(entry, quoted_ssid);
        if (match && (!line_end || (match < line_end))) {
            return 1;
        }
        entry = strstr(entry + 7, "+CWLAP:");
    }

    return 0;
}

static int esp32_wait_response(int timeout_ms)
{
    int total = 0;
    int elapsed = 0;

    memset(gEsp32RxBuf, 0, sizeof(gEsp32RxBuf));

    while (total < (ESP32_RX_BUF_SIZE - 1)) {
        int ret = esp32_read_byte(&gEsp32RxBuf[total], 100);
        if (ret == -ETIMEDOUT) {
            elapsed += 100;
            if (elapsed >= timeout_ms) {
                break;
            }
            if (total == 0) {
                continue;
            }
            break;
        }
        if (ret <= 0) {
            break;
        }

        total++;
        elapsed = 0;

        if (esp32_buffer_contains("\r\nOK\r\n") ||
            esp32_buffer_contains("\r\nERROR\r\n") ||
            esp32_buffer_contains("SEND OK") ||
            esp32_buffer_contains("SEND FAIL") ||
            esp32_buffer_contains("busy p") ||
            esp32_buffer_contains("ALREADY CONNECTED") ||
            esp32_buffer_contains("WIFI GOT IP") ||
            esp32_buffer_contains("+IPD,") ||
            gEsp32RxBuf[total - 1] == '>') {
            break;
        }
    }

    gEsp32RxBuf[total] = '\0';
    return total;
}

static int esp32_wait_wifi_join_response(int timeout_ms)
{
    int total = 0;
    int elapsed = 0;

    memset(gEsp32RxBuf, 0, sizeof(gEsp32RxBuf));

    while ((total < (ESP32_RX_BUF_SIZE - 1)) && (elapsed < timeout_ms)) {
        int ret = esp32_read_byte(&gEsp32RxBuf[total], 100);
        elapsed += 100;
        if (ret == -ETIMEDOUT) {
            continue;
        }
        if (ret <= 0) {
            break;
        }

        total++;

        if (esp32_buffer_contains("\r\nOK\r\n") ||
            esp32_buffer_contains("\r\nERROR\r\n") ||
            esp32_buffer_contains("FAIL") ||
            esp32_buffer_contains("+CWJAP:")) {
            break;
        }
    }

    gEsp32RxBuf[total] = '\0';
    return total;
}

static int esp32_wait_scan_response(int timeout_ms)
{
    int total = 0;
    int elapsed = 0;

    memset(gEsp32RxBuf, 0, sizeof(gEsp32RxBuf));

    while ((total < (ESP32_RX_BUF_SIZE - 1)) && (elapsed < timeout_ms)) {
        int ret = esp32_read_byte(&gEsp32RxBuf[total], 100);
        elapsed += 100;
        if (ret == -ETIMEDOUT) {
            continue;
        }
        if (ret <= 0) {
            break;
        }

        total++;

        if (esp32_buffer_contains("\r\nOK\r\n") ||
            esp32_buffer_contains("\r\nERROR\r\n")) {
            break;
        }
    }

    gEsp32RxBuf[total] = '\0';
    return total;
}

static int esp32_wait_pattern(const char *pattern, int timeout_ms)
{
    int rx_len = esp32_wait_response(timeout_ms);
    if (rx_len < 0) return rx_len;
    return esp32_buffer_contains(pattern) ? ESUCCESS : -ETIMEDOUT;
}

static int esp32_response_ok(void)
{
    if (esp32_buffer_contains("\r\nOK\r\n")) return ESUCCESS;
    if (esp32_buffer_contains("\r\nERROR\r\n")) return -EIO;
    return -EIO;
}

int esp32_init(void)
{
    xprintf("[ESP32] Using UART SCI%d for ESP32-C3\r\n", ESP32_UART_CHANNEL);
    esp32_start_background_rx();
    esp32_flush_rx();
    esp32_delay_ms(500);

    int ret = esp32_at_test();
    if (ret != ESUCCESS) {
        xprintf("[ESP32] AT test FAILED (%d). Check power, EN, RX/TX.\r\n", ret);
        return ret;
    }

    xprintf("[ESP32] AT test OK.\r\n");
    return ESUCCESS;
}

int esp32_at_test(void)
{
    esp32_flush_rx();
    esp32_send_cmd("AT\r\n");
    esp32_wait_response(TIMEOUT_AT_BASIC);
    return esp32_response_ok();
}

int esp32_reset(void)
{
    esp32_send_cmd("AT+RST\r\n");
    esp32_delay_ms(2000);

    for (int retry = 0; retry < 10; ++retry) {
        esp32_flush_rx();
        if (esp32_at_test() == ESUCCESS) {
            xprintf("[ESP32] Reset complete.\r\n");
            return ESUCCESS;
        }
        esp32_delay_ms(1000);
    }

    return -ETIMEDOUT;
}

int esp32_set_mode(int mode)
{
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "AT+CWMODE=%d\r\n", mode);
    esp32_send_cmd(cmd);
    esp32_wait_response(TIMEOUT_AT_BASIC);
    return esp32_response_ok();
}

int esp32_set_auto_connect(int enable)
{
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "AT+CWAUTOCONN=%d\r\n", enable ? 1 : 0);
    esp32_send_cmd(cmd);
    esp32_wait_response(TIMEOUT_AT_BASIC);
    return esp32_response_ok();
}

int esp32_set_sleep(int mode)
{
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "AT+SLEEP=%d\r\n", mode);
    esp32_send_cmd(cmd);
    esp32_wait_response(TIMEOUT_AT_BASIC);
    return esp32_response_ok();
}

int esp32_enable_station_dhcp(void)
{
    esp32_send_cmd("AT+CWDHCP=1,1\r\n");
    esp32_wait_response(TIMEOUT_AT_BASIC);
    return esp32_response_ok();
}

int esp32_scan_ap(const char *ssid)
{
    if (!ssid || !ssid[0]) return -EINVAL;

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "AT+CWLAP=\"%s\"\r\n", ssid);

    esp32_flush_rx();
    esp32_send_cmd(cmd);

    int rx_len = esp32_wait_scan_response(15000);
    if (rx_len <= 0) return -ETIMEDOUT;
    if (esp32_scan_result_contains_ssid(ssid)) return ESUCCESS;
    if (esp32_buffer_contains("\r\nOK\r\n")) return -ENODEV;
    return -EIO;
}

int esp32_connect_wifi(const char *ssid, const char *pwd)
{
    if (!ssid || !pwd) return -EINVAL;

    int ret = esp32_set_mode(1);
    if (ret != ESUCCESS) {
        xprintf("[ESP32] Set station mode FAILED\r\n");
        return ret;
    }

    esp32_set_sleep(0);
    esp32_enable_station_dhcp();

    ret = esp32_scan_ap(ssid);
    if (ret != ESUCCESS) {
        xprintf("[ESP32] Target AP not found by scan\r\n");
        return -ESP32_ERR_AP_NOT_FOUND;
    }

    esp32_set_auto_connect(0);
    esp32_disconnect_wifi();
    esp32_delay_ms(500);
    esp32_flush_rx();

    char cmd[160];
    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, pwd);

    esp32_flush_rx();
    xprintf("[ESP32] Connecting to WiFi: %s\r\n", ssid);
    esp32_send_cmd(cmd);

    int rx_len = esp32_wait_wifi_join_response(TIMEOUT_AT_CONNECT);
    if (rx_len <= 0) {
        xprintf("[ESP32] WiFi connect timeout\r\n");
        return -ETIMEDOUT;
    }

    if (esp32_buffer_contains("+CWJAP:1")) {
        xprintf("[ESP32] WiFi connection timeout\r\n");
        return -ESP32_ERR_CONNECT_TIMEOUT;
    }

    if (esp32_buffer_contains("+CWJAP:2")) {
        xprintf("[ESP32] WiFi wrong password/auth failed\r\n");
        return -ESP32_ERR_AUTH_FAIL;
    }

    if (esp32_buffer_contains("+CWJAP:3")) {
        xprintf("[ESP32] WiFi AP not found\r\n");
        return -ESP32_ERR_AP_NOT_FOUND;
    }

    if (esp32_buffer_contains("+CWJAP:4")) {
        xprintf("[ESP32] WiFi connection failed\r\n");
        return -ESP32_ERR_CONNECT_FAIL;
    }

    if (esp32_buffer_contains("FAIL") || esp32_buffer_contains("\r\nERROR\r\n")) {
        xprintf("[ESP32] WiFi connect FAILED\r\n");
        return -ECONNREFUSED;
    }

    if (esp32_buffer_contains("WIFI GOT IP") || esp32_buffer_contains("\r\nOK\r\n")) {
        xprintf("[ESP32] WiFi connected.\r\n");
        return ESUCCESS;
    }

    xprintf("[ESP32] Unexpected WiFi response: %s\r\n", gEsp32RxBuf);
    return -EIO;
}

int esp32_disconnect_wifi(void)
{
    esp32_send_cmd("AT+CWQAP\r\n");
    esp32_wait_response(TIMEOUT_AT_BASIC);
    return esp32_response_ok();
}

int esp32_get_connected_ssid(char *ssid_buf, int buf_len)
{
    if (!ssid_buf || buf_len <= 0) return -EINVAL;

    memset(ssid_buf, 0, buf_len);
    esp32_flush_rx();
    esp32_send_cmd("AT+CWJAP?\r\n");

    int rx_len = esp32_wait_response(3000);
    if (rx_len <= 0) return -EIO;

    const char *p = strstr((const char *)gEsp32RxBuf, "+CWJAP:\"");
    if (!p) return -EIO;

    p += 8;
    int i = 0;
    while (*p && *p != '"' && i < (buf_len - 1)) {
        ssid_buf[i++] = *p++;
    }
    ssid_buf[i] = '\0';
    return (i > 0) ? ESUCCESS : -EIO;
}

int esp32_get_ip(char *ip_buf, int buf_len)
{
    if (!ip_buf || buf_len <= 0) return -EINVAL;

    memset(ip_buf, 0, buf_len);
    esp32_flush_rx();
    esp32_send_cmd("AT+CIPSTA?\r\n");

    int rx_len = esp32_wait_response(TIMEOUT_AT_BASIC);
    if (rx_len <= 0) return -EIO;

    const char *p = strstr((const char *)gEsp32RxBuf, "+CIPSTA:ip:\"");
    if (!p) return -EIO;

    p += 12;
    int i = 0;
    while (*p && *p != '"' && i < (buf_len - 1)) {
        ip_buf[i++] = *p++;
    }
    ip_buf[i] = '\0';
    return ESUCCESS;
}

int esp32_tcp_connect(const char *host, int port)
{
    if (!host || port <= 0) return -EINVAL;

    char cmd[160];
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%d\r\n", host, port);

    esp32_flush_rx();
    esp32_send_cmd(cmd);

    int rx_len = esp32_wait_response(TIMEOUT_AT_TCP);
    if (rx_len <= 0) return -ETIMEDOUT;

    if (esp32_buffer_contains("\r\nOK\r\n") ||
        esp32_buffer_contains("CONNECT") ||
        esp32_buffer_contains("ALREADY CONNECTED")) {
        return ESUCCESS;
    }

    return -ECONNREFUSED;
}

int esp32_tcp_send(const uint8_t *data, int len)
{
    if (!data || len <= 0) return -EINVAL;

    char cmd[32];
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d\r\n", len);

    esp32_flush_rx();
    esp32_send_cmd(cmd);

    int ret = esp32_wait_pattern(">", TIMEOUT_AT_SEND);
    if (ret != ESUCCESS) {
        xprintf("[ESP32] CIPSEND prompt timeout\r\n");
        return ret;
    }

    esp32_send_data(data, len);

    ret = esp32_wait_pattern("SEND OK", TIMEOUT_AT_SEND);
    if (ret != ESUCCESS) {
        xprintf("[ESP32] TCP send FAILED\r\n");
        return -EIO;
    }

    return len;
}

int esp32_tcp_recv(uint8_t *buf, int max_len, int timeout_ms)
{
    if (!buf || max_len <= 0) return -EINVAL;

    int rx_len = esp32_wait_response(timeout_ms);
    if (rx_len <= 0) return rx_len;

    const char *p = strstr((const char *)gEsp32RxBuf, "+IPD,");
    if (!p) return 0;

    p += 5;
    int data_len = atoi(p);
    if (data_len <= 0) return 0;

    p = strchr(p, ':');
    if (!p) return 0;
    p++;

    int copy_len = data_len < max_len ? data_len : max_len;
    memcpy(buf, p, (size_t)copy_len);
    return copy_len;
}

void esp32_tcp_close(void)
{
    esp32_send_cmd("AT+CIPCLOSE\r\n");
    esp32_wait_response(TIMEOUT_AT_BASIC);
}

int esp32_http_get(const char *host, int port, const char *path,
                   char *resp_buf, int max_len, int timeout_ms)
{
    if (!host || !path || !resp_buf || max_len <= 0) return -EINVAL;

    memset(resp_buf, 0, max_len);

    xprintf("[HTTP] GET http://%s:%d%s\r\n", host, port, path);
    int ret = esp32_tcp_connect(host, port);
    if (ret != ESUCCESS) {
        xprintf("[HTTP] TCP connect to %s:%d FAILED\r\n", host, port);
        return ret;
    }

    char request[512];
    snprintf(request, sizeof(request),
             "GET %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Connection: close\r\n"
             "Accept: */*\r\n"
             "\r\n",
             path, host);

    int req_len = (int)strlen(request);
    ret = esp32_tcp_send((const uint8_t *)request, req_len);
    if (ret != req_len) {
        xprintf("[HTTP] Send request FAILED\r\n");
        esp32_tcp_close();
        return -EIO;
    }

    int total = 0;
    while (total < max_len - 1) {
        int chunk = esp32_tcp_recv((uint8_t *)&resp_buf[total],
                                   max_len - 1 - total,
                                   timeout_ms);
        if (chunk == -ETIMEDOUT || chunk == 0) {
            break;
        }
        if (chunk < 0) {
            esp32_tcp_close();
            return chunk;
        }
        total += chunk;
    }

    resp_buf[total] = '\0';
    esp32_tcp_close();
    xprintf("[HTTP] Received %d bytes\r\n", total);
    return total;
}

int esp32_http_post(const char *host, int port, const char *path,
                    const char *body, const char *content_type,
                    char *resp_buf, int max_len, int timeout_ms)
{
    if (!host || !path || !body || !resp_buf || max_len <= 0) return -EINVAL;

    memset(resp_buf, 0, max_len);

    xprintf("[HTTP] POST http://%s:%d%s\r\n", host, port, path);
    int ret = esp32_tcp_connect(host, port);
    if (ret != ESUCCESS) {
        xprintf("[HTTP] TCP connect FAILED\r\n");
        return ret;
    }

    char request[1024];
    int body_len = (int)strlen(body);
    const char *ct = content_type ? content_type : "application/json";
    snprintf(request, sizeof(request),
             "POST %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %d\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             path, host, ct, body_len, body);

    int req_len = (int)strlen(request);
    ret = esp32_tcp_send((const uint8_t *)request, req_len);
    if (ret != req_len) {
        xprintf("[HTTP] Send request FAILED\r\n");
        esp32_tcp_close();
        return -EIO;
    }

    int total = 0;
    while (total < max_len - 1) {
        int chunk = esp32_tcp_recv((uint8_t *)&resp_buf[total],
                                   max_len - 1 - total,
                                   timeout_ms);
        if (chunk == -ETIMEDOUT || chunk == 0) {
            break;
        }
        if (chunk < 0) {
            esp32_tcp_close();
            return chunk;
        }
        total += chunk;
    }

    resp_buf[total] = '\0';
    esp32_tcp_close();
    xprintf("[HTTP] Received %d bytes\r\n", total);
    return total;
}
