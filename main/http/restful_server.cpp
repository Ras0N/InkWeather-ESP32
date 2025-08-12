#include "restful_server.hpp"
/**
 * @brief ESP32_Restful_Server类的构造函数
 *
 * 该构造函数用于初始化ESP32 RESTful服务器对象。
 * 默认为80端口，等待超时时间为5s.
 */
ESP32_Restful_Server::ESP32_Restful_Server()
{
    ;
}
/**
 * @brief ESP32_Restful_Server类的构造函数
 * @param port 服务器监听的端口号
 * @param timeout_in_seconds 服务器超时时间（秒）
 */
ESP32_Restful_Server::ESP32_Restful_Server(int port, int timeout_in_seconds)
{
    this->server_port = port;
    this->server_timeout = timeout_in_seconds;
};
/**
 * @brief 初始化ESP32的mDNS服务
 *
 * 该函数用于初始化并配置ESP32设备的mDNS服务，使其能够通过域名被发现和访问。
 * 设置主机名、实例名，并添加HTTP服务记录以便网络发现。
 *
 * @return esp_err_t 返回ESP-IDF错误代码，ESP_OK表示初始化成功，其他值表示初始化失败
 */
esp_err_t ESP32_Restful_Server::initialise_mDNS()
{
    mdns_init();
    mdns_hostname_set("ESP32-InkWeather");
    mdns_instance_name_set("ESP32 InkWeather http server");
    mdns_txt_item_t serviceTxtdata[] = {
        {"board", "esp32"},
        {"path", "/"}};

    esp_err_t ret = mdns_service_add(
        "ESP32-InkWeather",
        "_http",
        "_tcp",
        this->server_port,
        serviceTxtdata,
        sizeof(serviceTxtdata) / sizeof(serviceTxtdata[0]));
    return ret;
}
/**
 * @brief 初始化挂载点，注册半主机驱动
 *
 * @param mount_point 挂载点路径
 * @return esp_err_t 返回操作结果，ESP_OK表示成功，其他值表示失败
 */
esp_err_t ESP32_Restful_Server::init_mount_points(char *mount_point)
{
    esp_err_t ret = esp_vfs_semihost_register(mount_point);
    if (ret != ESP_OK)
    {
        ESP_LOGE(this->TAG, "Unable to register semihost driver: %s!", esp_err_to_name(ret));
        return ESP_FAIL;
    }
    return ESP_OK;
}

/**
 * @brief 初始化ESP32 Restful服务器
 *
 * 该函数负责初始化服务器所需的各种组件，包括mDNS服务和文件系统挂载点
 *
 * @param mount_point 文件系统挂载点路径，挂载点位于内部虚拟文件系统中，类似于linux系统下的/www/html文件夹
 * @return esp_err_t 初始化结果，ESP_OK表示成功，其他值表示失败
 */
esp_err_t ESP32_Restful_Server::server_init(char *mount_point)
{
    esp_err_t ret = this->initialise_mDNS();
    if (ret != ESP_OK)
    {
        return ret;
    }
    // ret = this->init_mount_points(mount_point);
    // if (ret != ESP_OK)
    // {
    // return ret;
    // }
    return ESP_OK;
}
/**
 * @brief 设置HTTP GET请求的回调函数
 *
 * @param http_function_get HTTP GET请求回调函数指针
 *
 * @return 无返回值
 *
 * 该函数用于设置处理HTTP GET请求的回调函数，当服务器接收到GET请求时会调用此函数
 */
void ESP32_Restful_Server::set_http_function_get(http_event_call_back http_function_get)
{
    this->http_function_get = http_function_get;
}
/**
 * @brief 设置HTTP POST请求的回调函数
 *
 * 该函数用于设置处理HTTP POST请求的回调函数，当服务器接收到POST请求时会调用此函数
 *
 * @param http_function_post HTTP POST请求回调函数指针
 * @return 无返回值
 */
void ESP32_Restful_Server::set_http_function_post(http_event_call_back http_function_post)
{
    this->http_function_post = http_function_post;
}
/**
 * @brief 启动 RESTful 服务器
 *
 * 该函数用于初始化并启动一个基于 HTTP 的 RESTful 服务器。它会注册两个 API 接口：
 * - GET 请求接口：/api/v1/info
 * - POST 请求接口：/api/v1/systemset
 *
 * 同时，服务器将挂载静态文件目录 `/www`，并使用指定的端口进行监听。
 *
 * @return
 *     - ESP_OK: 成功启动服务器
 *     - 其他: 启动失败，返回对应错误码（如内存分配失败、HTTP 服务启动失败等）
 */
esp_err_t ESP32_Restful_Server::server_start()
{
    char *mountpoint = (char *)"/www"; // 默认挂在在/www目录下
    esp_err_t ret = this->server_init(mountpoint);
    if (ret != ESP_OK)
    {
        ESP_LOGE(this->TAG, "Failed to start server");
        return ret;
    }
    rest_server_context_t *rest_context = (rest_server_context *)calloc(1, sizeof(rest_server_context_t));
    if (rest_context == nullptr)
    {
        ESP_LOGE(this->TAG, "Failed to allocate memory for rest context");
        return ESP_FAIL;
    }
    strlcpy(rest_context->base_path, mountpoint, sizeof(rest_context->base_path));

    this->server_config.uri_match_fn = httpd_uri_match_wildcard;
    this->server_config.linger_timeout = this->server_timeout;

    ESP_LOGI(this->TAG, "Starting server on port: '%d'", this->server_port);
    ret = httpd_start(&(this->server), &(this->server_config));
    if (ret != ESP_OK)
    {
        ESP_LOGE(this->TAG, "Failed to start http server: %d", ret);
        free(rest_context);
        return ret;
    }
    bool register_ok = false;
    if (this->http_function_get != nullptr)
    {
        // httpd_uri_t system_get_uri = {
        //     .uri = (char *)"/api/v1/info",
        //     .method = HTTP_GET,
        //     .handler = this->http_function_get,
        //     .user_ctx = rest_context};
        httpd_uri_t system_get_uri = {};
        system_get_uri.uri = this->get_function_entry;
        system_get_uri.method = HTTP_GET;
        system_get_uri.handler = this->http_function_get;
        system_get_uri.user_ctx = rest_context;
        ret = httpd_register_uri_handler((this->server), &system_get_uri);
        register_ok |= ret == ESP_OK ? true : false;
        if (ret != ESP_OK)
        {
            ESP_LOGE(this->TAG, "Register GET URI failed");
        }
    }
    if (this->http_function_post != nullptr)
    {
        // httpd_uri_t system_post_uri = {
        //     .uri = (char *)"/api/v1/systemset",
        //     .method = HTTP_POST,
        //     .handler = this->http_function_post,
        //     .user_ctx = rest_context};

        httpd_uri_t system_post_uri = {};
        bzero(&system_post_uri, sizeof(httpd_uri_t));
        system_post_uri.uri = this->post_function_entry;
        system_post_uri.method = HTTP_POST;
        system_post_uri.handler = this->http_function_post;
        system_post_uri.user_ctx = rest_context;

        ret = httpd_register_uri_handler(this->server, &system_post_uri);
        register_ok |= ret == ESP_OK ? true : false;
        if (ret != ESP_OK)
        {
            ESP_LOGE(this->TAG, "Register POST URI failed");
        }
    }
    if (register_ok != true)
    {
        httpd_stop(this->server);
        free(rest_context);
        return ESP_FAIL;
    }
    return ESP_OK;
}