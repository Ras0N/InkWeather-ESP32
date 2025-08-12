#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "nvs_flash.h"

#include "http/restful_server.hpp"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define MAX_CONNECT_TRY 3

#define WIFI_SSID "WLAN_SSID"
#define WIFI_PASS "PASSWORD"
static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else
    {
        if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
        {
            ESP_LOGI("MAIN", "Failed to connect to the AP. Attemp count: %d", s_retry_num);
            if (s_retry_num < MAX_CONNECT_TRY)
            {
                esp_wifi_connect();
                s_retry_num++;
                ESP_LOGI("MAIN", "retry to connect to the AP");
            }
            else
            {
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            }
        }
        else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
        {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            ESP_LOGI("MAIN", "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
            s_retry_num = 0;
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }
        else
        {
            ;
        }
    }
}
static esp_err_t http_function_get_static(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "message", "hello from esp32S3");
    cJSON_AddStringToObject(root, "ExterInfo", "A String created by static funtion");
    const char *aJsonString = cJSON_Print(root);
    httpd_resp_sendstr(req, aJsonString);
    free((void *)aJsonString);
    cJSON_Delete(root);
    return ESP_OK;
}
static esp_err_t http_function_post_static(httpd_req_t *req)
{
    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    int received = 0;
    if (total_len >= SCRATCH_BUFSIZE)
    {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len)
    {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0)
        {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad Request");
            return ESP_FAIL;
        }
        cur_len += received;
    }

    if (total_len == 0)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Unknown request");
        return ESP_OK;
    }
    buf[total_len] = '\0';
    ESP_LOGI("MAIN", "Received HTTP POST request for %s", buf);

    httpd_resp_sendstr(req, "Post control value successfully");
    return ESP_OK;
}

esp_err_t wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();
    if (esp_netif_init() != ESP_OK)
    {
        ESP_LOGE("MAIN", "esp_netif_init failed");
        return ESP_FAIL;
    }
    if (esp_event_loop_create_default() != ESP_OK)
    {
        ESP_LOGE("MAIN", "esp_event_loop_create_default failed");
        return ESP_FAIL;
    }
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&cfg) != ESP_OK)
    {
        ESP_LOGE("MAIN", "esp_wifi_init failed");
        return ESP_FAIL;
    }
    esp_event_handler_instance_t instance_any_id, instance_got_ip;
    if (ESP_OK != esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id))
    {
        ESP_LOGE("MAIN", "esp_event_handler_instance_register failed");
    }
    if (ESP_OK != esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip))
    {
        ESP_LOGE("MAIN", "esp_event_handler_instance_register failed");
    }
    wifi_config_t wifi_cfg;
    bzero(&wifi_cfg, sizeof(wifi_config_t));
    memccpy(wifi_cfg.sta.ssid, WIFI_SSID, '\0', sizeof(wifi_cfg.sta.ssid));
    memccpy(wifi_cfg.sta.password, WIFI_PASS, '\0', sizeof(wifi_cfg.sta.password));
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_cfg.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
    memccpy(wifi_cfg.sta.sae_h2e_identifier, "", '\0', sizeof(wifi_cfg.sta.sae_h2e_identifier));
    // wifi_cfg.sta.sae_h2e_identifier = ;
    if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK)
    {
        ESP_LOGE("MAIN", "esp_wifi_set_mode failed");
    };
    if (esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg) != ESP_OK)
    {
        ESP_LOGE("MAIN", "esp_wifi_set_config failed");
    };
    if (esp_wifi_start() != ESP_OK)
    {
        ESP_LOGE("MAIN", "esp_wifi_start failed");
    };
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);
    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI("MAIN", "connected to ap SSID:%s password:%s", wifi_cfg.sta.ssid, wifi_cfg.sta.password);
        return ESP_OK;
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI("MAIN", "Failed to connect to SSID:%s, password:%s", wifi_cfg.sta.ssid, wifi_cfg.sta.password);
        return ESP_FAIL;
    }
    else
    {
        ESP_LOGE("MAIN", "UNEXPECTED EVENT");
        return ESP_FAIL;
    }
    return ESP_FAIL;
}
extern "C" void app_main(void)
{
    printf("entry of esp32s3!");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI("MAIN", "ESP_WIFI_MODE_STA");
    if (wifi_init_sta() == ESP_OK)
    {
        ESP32_Restful_Server restful_server = ESP32_Restful_Server();
        restful_server.set_http_function_get(http_function_get_static);
        restful_server.set_http_function_post(http_function_post_static);
        restful_server.server_start();
    }
    else
    {
        esp_restart();
    }
}