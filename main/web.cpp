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
    auto changes = settings.updateFromJson(content);
	if (changes.empty()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to parse JSON");
        return ESP_FAIL;
    }
	for (const auto& [key, value] : changes) {
        ESP_LOGI(TAG, "key %s was updated to %s", key.c_str(), value.c_str());
	}
    httpd_resp_send(req, nullptr, 0);  // Send HTTP 200 OK with no further content
    return ESP_OK;
}

httpd_uri_t post_settings_update = {
	.uri = "/settings",
	.method = HTTP_POST,
	.handler = handle_settings_update,
	.user_ctx = nullptr
};

WebServer::WebServer(WebContext& ctx, uint16_t port) : port(port) {
	ESP_LOGI(TAG, "Starting web server  on port %d", port);
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = port;

    if (httpd_start(&server, &config) == ESP_OK) {
		post_settings_update.user_ctx = &ctx;
		httpd_register_uri_handler(server, &post_settings_update);
    }
}
