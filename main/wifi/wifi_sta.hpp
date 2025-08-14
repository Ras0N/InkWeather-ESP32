/****************************
 * wifi_sta.hpp
 *
 *  Description: ESP Wifi Connection Functions
 *
 *  Version: 1.0
 *  Created on: 2025/08/14
 *  Author: Ras0N
 */

#ifndef WIFI_STA_HPP
#define WIFI_STA_HPP
#include "sdkconfig.h"
#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_partition.h"
#include "wear_levelling.h"
#include "esp_vfs_fat.h"
#endif

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define MAX_CONNECT_TRY 3

typedef struct
{
    char ssid[32];
    char password[64];
} WIFI_CONNECT_INFO, *PWIFI_CONNECT_INFO;

typedef void (*WIFI_EVENT_HANDLER)(void *, esp_event_base_t, int32_t, void *);
static bool partition_mounted = false;
static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
static void default_wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
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
class ESP32_WIFI_STA
{
public:
    ESP32_WIFI_STA();
    esp_err_t wifi_init_sta();
    esp_err_t save_connect_point();
    esp_err_t load_connect_point();
    void set_wifi_event_handler(WIFI_EVENT_HANDLER event_handler);
    void set_wifi_connect_point(const char *ssid, const char *password);
    esp_err_t re_connect_wifi();

private:
    WIFI_CONNECT_INFO info;
    WIFI_EVENT_HANDLER wifi_event_handler = default_wifi_event_handler;
    esp_err_t init_partition_data();
};