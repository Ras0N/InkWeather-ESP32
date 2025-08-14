#include "nvs_flash.h"
#include "wifi/wifi_sta.hpp"
#include "http/restful_server.hpp"

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
    ESP32_WIFI_STA sta;
    // sta.load_connect_point();
    if (sta.wifi_init_sta() == ESP_OK)
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