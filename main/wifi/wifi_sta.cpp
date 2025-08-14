#include "wifi/wifi_sta.hpp"
ESP32_WIFI_STA::ESP32_WIFI_STA()
{
    bzero(&(this->info), sizeof(WIFI_CONNECT_INFO));
    if (this->init_partition_data() == ESP_OK)
    {
        partition_mounted = true;
    }
}
esp_err_t ESP32_WIFI_STA::load_connect_point()
{
    if (partition_mounted)
    {
        FILE *fid = fopen("/int_flash/wifi_info", "rb");
        fread(&(this->info), sizeof(WIFI_CONNECT_INFO), 1, fid);
        fclose(fid);
        return ESP_OK;
    }
    return ESP_FAIL;
}
esp_err_t ESP32_WIFI_STA::save_connect_point()
{
    if (partition_mounted)
    {
        FILE *fid = fopen("/int_flash/wifi_info", "wb");
        fwrite(&(this->info), sizeof(WIFI_CONNECT_INFO), 1, fid);
        fclose(fid);
        return ESP_OK;
    }
    return ESP_FAIL;
}
void ESP32_WIFI_STA::set_wifi_connect_point(const char *ssid, const char *password)
{
    bzero(&(this->info), sizeof(WIFI_CONNECT_INFO));
    memccpy(&(this->info.ssid), ssid, '\0', 32);
    memccpy(&(this->info.password), password, '\0', 64);
}
void ESP32_WIFI_STA::set_wifi_event_handler(WIFI_EVENT_HANDLER event_handler)
{
    this->wifi_event_handler = event_handler;
}
esp_err_t ESP32_WIFI_STA::wifi_init_sta(void)
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
    if (ESP_OK != esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, this->wifi_event_handler, NULL, &instance_any_id))
    {
        ESP_LOGE("MAIN", "esp_event_handler_instance_register failed");
    }
    if (ESP_OK != esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, this->wifi_event_handler, NULL, &instance_got_ip))
    {
        ESP_LOGE("MAIN", "esp_event_handler_instance_register failed");
    }
    wifi_config_t wifi_cfg;
    bzero(&wifi_cfg, sizeof(wifi_config_t));
    memccpy(wifi_cfg.sta.ssid, this->info.ssid, '\0', sizeof(wifi_cfg.sta.ssid));
    memccpy(wifi_cfg.sta.password, this->info.password, '\0', sizeof(wifi_cfg.sta.password));
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
esp_err_t ESP32_WIFI_STA::init_partition_data()
{
    const esp_partition_t *PARTITION = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, "user_data");
    if (PARTITION != nullptr)
    {
        wl_handle_t wl_handle;
        esp_vfs_fat_mount_config_t mount_config = {};
        bzero(&mount_config, sizeof(esp_vfs_fat_mount_config_t));
        mount_config.max_files = 4;
        mount_config.format_if_mount_failed = true;
        mount_config.allocation_unit_size = CONFIG_WL_SECTOR_SIZE;
        esp_err_t ret = esp_vfs_fat_spiflash_mount_rw_wl("/int_flash", "user_data", &mount_config, &wl_handle);
        if (ret != ESP_OK)
        {
            ESP_LOGE("MAIN", "Failed to mount FATFS (%s)", esp_err_to_name(ret));
        }
        ESP_LOGI("MAIN", "Internal Flash is mounted at /int_flash!");
        partition_mounted = true;
        return ESP_OK;
    }
    return ESP_FAIL;
}