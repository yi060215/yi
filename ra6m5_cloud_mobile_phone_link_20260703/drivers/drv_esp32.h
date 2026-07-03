#ifndef __DRV_ESP32_H
#define __DRV_ESP32_H

#include <stdint.h>
#include "r_uart_api.h"

int  esp32_init(void);

int  esp32_at_test(void);
int  esp32_diag_at(char *resp_buf, int buf_len);
int  esp32_reset(void);
int  esp32_set_mode(int mode);
int  esp32_set_auto_connect(int enable);
int  esp32_set_sleep(int mode);
int  esp32_enable_station_dhcp(void);

int  esp32_scan_ap(const char *ssid);
int  esp32_connect_wifi(const char *ssid, const char *pwd);
int  esp32_disconnect_wifi(void);
int  esp32_get_ip(char *ip_buf, int buf_len);
int  esp32_get_connected_ssid(char *ssid_buf, int buf_len);

int  esp32_tcp_connect(const char *host, int port);
int  esp32_tcp_send(const uint8_t *data, int len);
int  esp32_tcp_recv(uint8_t *buf, int max_len, int timeout_ms);
void esp32_tcp_close(void);

int  esp32_http_get(const char *host, int port, const char *path,
                    char *resp_buf, int max_len, int timeout_ms);
int  esp32_http_post(const char *host, int port, const char *path,
                     const char *body, const char *content_type,
                     char *resp_buf, int max_len, int timeout_ms);

void esp32_uart_callback(uart_callback_args_t *p_args);

#endif /* __DRV_ESP32_H */
