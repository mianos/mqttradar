#include <algorithm> 
#include <regex>
#include "esp_log.h"
#include <string>
#include <memory>

#include "SettingsManager.h"
#include "web.h"


#define GET_CONTEXT(req, ctx) \
    auto* ctx = static_cast<WebContext*>(req->user_ctx); \
    if (!ctx) { \
		ESP_LOGE(TAG,"ctx null?"); \
        httpd_resp_send_500(req); \
        return ESP_FAIL; \
    }

static const char* TAG = "Web";

esp_err_t handle_settings_update(httpd_req_t *req) {
    if (!req) return ESP_FAIL; // Early exit on null request

    // Prepare buffer for receiving request data
    char content[1024];
    int received = httpd_req_recv(req, content, sizeof(content) - 1);
    if (received <= 0) { // Handle errors or no data received
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive data");
        return ESP_FAIL;
    }
    content[received] = '\0'; // Null terminate the string

    // Get context which should contain a SettingsManager
    auto settings = static_cast<WebContext *>(req->user_ctx)->settings;
    auto changes = settings->updateFromJson(content);
	httpd_resp_set_type(req, "application/json");
	std::string jsonResponse = settings->convertChangesToJson(changes);
	httpd_resp_send(req, jsonResponse.c_str(), jsonResponse.length());
	return ESP_OK;
}

httpd_uri_t post_settings_update = {
	.uri = "/settings",
	.method = HTTP_POST,
	.handler = handle_settings_update,
	.user_ctx = nullptr
};

esp_err_t handle_settings_get(httpd_req_t *req) {
    if (!req) return ESP_FAIL; // Early exit on null request

    // Get context which should contain a SettingsManager
    auto settings = static_cast<WebContext *>(req->user_ctx)->settings;

    // Retrieve current settings as JSON
    std::string jsonResponse = settings->toJson(); // Assume toJson method that serializes settings to JSON string
    if (jsonResponse.empty()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to retrieve settings");
        return ESP_FAIL;
    } else {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, jsonResponse.c_str(), jsonResponse.length());
        return ESP_OK;
    }
}

httpd_uri_t get_settings = {
    .uri = "/settings",
    .method = HTTP_GET,
    .handler = handle_settings_get,
    .user_ctx = nullptr
};

// Handler function for rebooting the ESP
esp_err_t handle_reboot(httpd_req_t *req) {
    if (!req) return ESP_FAIL; // Early exit on null request

    // Send confirmation message back to the client
    httpd_resp_set_type(req, "application/json");
    const char *response = "{\"message\": \"Rebooting now...\"}";
    httpd_resp_send(req, response, strlen(response));

    // Delay to ensure the response is sent before rebooting
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Command to reboot the ESP
    esp_restart();

    return ESP_OK;
}

// Endpoint structure for the reboot function
httpd_uri_t post_reboot = {
    .uri = "/reboot",
    .method = HTTP_POST,
    .handler = handle_reboot,
    .user_ctx = nullptr
};


WebServer::WebServer(WebContext& ctx, uint16_t port) : port(port) {
	ESP_LOGI(TAG, "Starting web server  on port %d", port);
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = port;

    if (httpd_start(&server, &config) == ESP_OK) {
		post_settings_update.user_ctx = &ctx;
		httpd_register_uri_handler(server, &post_settings_update);
		get_settings.user_ctx = &ctx;
		httpd_register_uri_handler(server, &get_settings);
		post_reboot.user_ctx = &ctx;
		httpd_register_uri_handler(server, &post_reboot);
    }
}
