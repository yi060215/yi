#ifndef __DRV_WIFI_H
#define __DRV_WIFI_H

#include <stdint.h>

#define WIFI_ERR_STAGE_NONE             0
#define WIFI_ERR_STAGE_CONNECT_AP       2
#define WIFI_ERR_STAGE_QUERY_SSID       3
#define WIFI_ERR_STAGE_SSID_MISMATCH    4
#define WIFI_ERR_STAGE_GET_IP           5
#define WIFI_ERR_STAGE_AP_NOT_FOUND     6
#define WIFI_ERR_STAGE_AUTH_FAIL        7
#define WIFI_ERR_STAGE_CONNECT_TIMEOUT  8
#define WIFI_ERR_STAGE_CONNECT_FAIL     9

int  wifi_init(void);
int  wifi_connect(const char *ssid, const char *pwd);
int  wifi_last_error_stage(void);
int  wifi_is_connected(void);
int  wifi_get_ip(char *ip, int len);
void wifi_disconnect(void);

int  wifi_http_get(const char *url, char *resp, int max_len);
int  wifi_http_post_json(const char *url, const char *json_body,
                         char *resp, int max_len);
int  cloud_set_server(const char *host, int port);

int  cloud_fetch_questions(char *resp, int max_len);
int  cloud_submit_diagnosis(const char *answers_json, char *resp, int max_len);
int  cloud_send_sms(const char *phone, const char *report_id);
int  cloud_get_report_url(const char *report_id, char *url_buf, int max_len);

#ifndef CLOUD_API_HOST
#define CLOUD_API_HOST  "your-server.example.com"
#endif

#ifndef CLOUD_API_PORT
#define CLOUD_API_PORT  80
#endif

#endif /* __DRV_WIFI_H */
