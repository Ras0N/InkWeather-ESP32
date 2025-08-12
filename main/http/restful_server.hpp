/*
 *-----------------------------------------------------
 *  restful_server.hpp
 *
 *  Description:  ESP32 restful server over HTTP
 *
 *  Version: 1.0
 *  Created on: 2025/0801
 *  Author: Ras0N
 *-----------------------------------------------------
 */

#ifndef RESTFUL_SERVER_H
#define RESTFUL_SERVER_H
#include "sdkconfig.h"
// #include "esp_mac.h"
#include "esp_https_server.h"
#include "esp_log.h"
#include "mdns.h"
#include "esp_vfs_semihost.h"
#include "esp_vfs_fat.h"
#include "esp_spiffs.h"
#include "cJSON.h"
#endif
class ESP32_Restful_Server
{
public:
    using http_event_call_back = esp_err_t (*)(httpd_req_t *);
    int server_port = 80;
    int server_timeout = 5;
    const char *TAG = "InkWeather Restful";
    const char *get_function_entry = "/api/v1/info";
    const char *post_function_entry = "/api/v1/systemset";
    ESP32_Restful_Server(int port, int timeout_in_seconds);
    ESP32_Restful_Server();
    esp_err_t server_start();
    void set_http_function_get(http_event_call_back http_function_get);
    void set_http_function_post(http_event_call_back http_function_post);

private:
    esp_err_t server_init(char *mount_point);
    httpd_handle_t server = nullptr;
    httpd_config_t server_config = HTTPD_DEFAULT_CONFIG();
    esp_err_t initialise_mDNS(void);
    esp_err_t init_mount_points(char *mount_point);
    // 挂载点：/api/v1/info
    http_event_call_back http_function_get = nullptr; // 基础get函数入口，需要外部实现
    // 挂载点：/api/v1/systemset
    http_event_call_back http_function_post = nullptr; // 基础post函数入口，需要外部实现
};
#define SCRATCH_BUFSIZE (1024)
typedef struct rest_server_context
{
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_BUFSIZE];
} rest_server_context_t;