/**
 * drv_wifi.c - WiFi 高层 API
 *
 * 在 ESP32 AT 驱动之上封装业务语义的 WiFi 操作。
 * 所有的云端 API URL 通过宏 CLOUD_API_HOST 配置。
 */

#include "drv_wifi.h"
#include "drv_esp32.h"
#include "libs.h"
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ================================================================
 *  内部状态
 * ================================================================ */

static int  gWifiConnected  = 0;
static int  gWifiLastErrorStage = WIFI_ERR_STAGE_NONE;
static char gLocalIp[16]    = {0};
static char gCurrentSsid[64] = {0};
static char gCloudHost[128] = CLOUD_API_HOST;
static int  gCloudPort      = CLOUD_API_PORT;

/* ================================================================
 *  WiFi 配网
 * ================================================================ */

int wifi_init(void)
{
    int ret = esp32_init();
    if (ret != ESUCCESS) {
        xprintf("[WiFi] ESP32 init FAILED (%d)\r\n", ret);
        return ret;
    }
    xprintf("[WiFi] ESP32 initialized.\r\n");
    return ESUCCESS;
}

int wifi_connect(const char *ssid, const char *pwd)
{
    if (!ssid || !pwd) return -EINVAL;

    gWifiLastErrorStage = WIFI_ERR_STAGE_NONE;

    int ret = esp32_connect_wifi(ssid, pwd);
    if (ret != ESUCCESS) {
        xprintf("[WiFi] Connect to '%s' FAILED\r\n", ssid);
        gWifiConnected = 0;
        if (ret == -201) {
            gWifiLastErrorStage = WIFI_ERR_STAGE_AP_NOT_FOUND;
        } else if (ret == -202) {
            gWifiLastErrorStage = WIFI_ERR_STAGE_AUTH_FAIL;
        } else if (ret == -203) {
            gWifiLastErrorStage = WIFI_ERR_STAGE_CONNECT_TIMEOUT;
        } else if (ret == -204) {
            gWifiLastErrorStage = WIFI_ERR_STAGE_CONNECT_FAIL;
        } else {
            gWifiLastErrorStage = WIFI_ERR_STAGE_CONNECT_AP;
        }
        return ret;
    }

    ret = esp32_get_connected_ssid(gCurrentSsid, sizeof(gCurrentSsid));
    if ((ret == ESUCCESS) && (strcmp(gCurrentSsid, ssid) != 0)) {
        xprintf("[WiFi] Connected SSID mismatch: %s\r\n", gCurrentSsid);
        gWifiConnected = 0;
        gWifiLastErrorStage = WIFI_ERR_STAGE_SSID_MISMATCH;
        return -ENOTCONN;
    }

    /* 获取 IP */
    ret = esp32_get_ip(gLocalIp, sizeof(gLocalIp));
    if (ret == ESUCCESS) {
        xprintf("[WiFi] IP: %s\r\n", gLocalIp);
    } else {
        xprintf("[WiFi] Get IP FAILED\r\n");
        gWifiConnected = 0;
        gWifiLastErrorStage = WIFI_ERR_STAGE_GET_IP;
        return ret;
    }

    gWifiConnected = 1;
    return ESUCCESS;
}

int wifi_last_error_stage(void)
{
    return gWifiLastErrorStage;
}

int wifi_is_connected(void)
{
    return gWifiConnected;
}

int wifi_get_ip(char *ip, int len)
{
    if (!ip || len <= 0) return -EINVAL;
    if (!gWifiConnected) return -ENOTCONN;
    strncpy(ip, gLocalIp, len - 1);
    return ESUCCESS;
}

void wifi_disconnect(void)
{
    esp32_disconnect_wifi();
    gWifiConnected = 0;
    memset(gLocalIp, 0, sizeof(gLocalIp));
}

/* ---- 内部 URL 解析 ---- */
static void parse_url_internal(const char *url, char *host, int host_len,
                               int *port, char *path, int path_len)
{
    const char *p = url;
    const char *host_begin;
    const char *host_end;
    const char *colon = NULL;

    memset(host, 0, host_len);
    memset(path, 0, path_len);
    if (port) *port = 80;

    if (strncmp(p, "http://", 7) == 0)  p += 7;
    if (strncmp(p, "https://", 8) == 0) p += 8;

    const char *slash = strchr(p, '/');
    host_begin = p;
    if (slash) {
        host_end = slash;
        int hlen = (int)(host_end - host_begin);
        if (hlen >= host_len) hlen = host_len - 1;
        strncpy(host, host_begin, hlen);
        strncpy(path, slash, path_len - 1);
    } else {
        host_end = p + strlen(p);
        strncpy(host, host_begin, host_len - 1);
        strncpy(path, "/", path_len - 1);
    }

    for (const char *it = host_begin; it < host_end; ++it) {
        if (*it == ':') {
            colon = it;
            break;
        }
    }

    if (colon) {
        int hlen = (int)(colon - host_begin);
        if (hlen >= host_len) hlen = host_len - 1;
        memset(host, 0, host_len);
        strncpy(host, host_begin, hlen);
        if (port) {
            int parsed_port = atoi(colon + 1);
            if (parsed_port > 0) {
                *port = parsed_port;
            }
        }
    }
}

int cloud_set_server(const char *host, int port)
{
    if (!host || !host[0]) return -EINVAL;
    if (port <= 0) return -EINVAL;

    memset(gCloudHost, 0, sizeof(gCloudHost));
    strncpy(gCloudHost, host, sizeof(gCloudHost) - 1);
    gCloudPort = port;
    return ESUCCESS;
}

/* ================================================================
 *  HTTP 通用接口
 * ================================================================ */

int wifi_http_get(const char *url, char *resp, int max_len)
{
    if (!url || !resp || max_len <= 0) return -EINVAL;
    if (!gWifiConnected) return -ENOTCONN;

    char host[128], path[256];
    int port = 80;
    parse_url_internal(url, host, sizeof(host), &port, path, sizeof(path));

    return esp32_http_get(host, port, path, resp, max_len, 15000);
}

int wifi_http_post_json(const char *url, const char *json_body,
                        char *resp, int max_len)
{
    if (!url || !json_body || !resp || max_len <= 0) return -EINVAL;
    if (!gWifiConnected) return -ENOTCONN;

    char host[128], path[256];
    int port = 80;
    parse_url_internal(url, host, sizeof(host), &port, path, sizeof(path));

    return esp32_http_post(host, port, path, json_body,
                           "application/json; charset=utf-8",
                           resp, max_len, 15000);
}

/* ================================================================
 *  云服务业务接口
 * ================================================================ */

/**
 * 构建完整的云端 API URL
 * 例: CLOUD_API_HOST="192.168.1.100:5000" → "http://192.168.1.100:5000/api/questions"
 */
static void build_cloud_url(char *url, int max_len, const char *endpoint)
{
    snprintf(url, max_len, "http://%s:%d%s", gCloudHost, gCloudPort, endpoint);
}

int cloud_fetch_questions(char *resp, int max_len)
{
    char url[256];
    build_cloud_url(url, sizeof(url), "/api/questions");
    xprintf("[Cloud] Fetching questions from %s ...\r\n", url);
    return wifi_http_get(url, resp, max_len);
}

int cloud_submit_diagnosis(const char *answers_json, char *resp, int max_len)
{
    char url[256];
    build_cloud_url(url, sizeof(url), "/api/diagnosis");
    xprintf("[Cloud] Submitting diagnosis...\r\n");
    return wifi_http_post_json(url, answers_json, resp, max_len);
}

int cloud_fetch_test_record(char *resp, int max_len)
{
    char url[256];
    build_cloud_url(url, sizeof(url), "/api/test-record");
    xprintf("[Cloud] Fetching test record...\r\n");
    return wifi_http_get(url, resp, max_len);
}

int cloud_upload_record(const char *record_json, char *resp, int max_len)
{
    char url[256];
    build_cloud_url(url, sizeof(url), "/api/upload-record");
    xprintf("[Cloud] Uploading record...\r\n");
    return wifi_http_post_json(url, record_json, resp, max_len);
}

int cloud_send_sms(const char *phone, const char *report_id)
{
    if (!phone || !report_id) return -EINVAL;

    char json[256];
    char url[256];
    char resp[512];

    snprintf(json, sizeof(json),
             "{\"phone\":\"%s\",\"report_id\":\"%s\"}", phone, report_id);
    build_cloud_url(url, sizeof(url), "/api/send-sms");

    xprintf("[Cloud] Sending SMS to %s ...\r\n", phone);
    int ret = wifi_http_post_json(url, json, resp, sizeof(resp));
    if (ret > 0) {
        xprintf("[Cloud] SMS response: %s\r\n", resp);
        return ESUCCESS;
    }
    return ret;
}

int cloud_get_report_url(const char *report_id, char *url_buf, int max_len)
{
    if (!report_id || !url_buf || max_len <= 0) return -EINVAL;

    /* 报告 URL 直接拼接, 不调用 API */
    snprintf(url_buf, max_len, "http://%s:%d/report/%s",
             gCloudHost, gCloudPort, report_id);
    return ESUCCESS;
}
