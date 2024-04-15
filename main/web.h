#pragma once
#include <string>
#include "esp_log.h"
#include "esp_http_server.h"
#include "SettingsManager.h"

struct WebContext {
	SettingsManager& settings;
};

class WebServer {
public:
    WebServer(WebContext &wc, uint16_t port=80);

private:
    httpd_handle_t server = nullptr;
    static esp_err_t getHandler(httpd_req_t *req);
    static esp_err_t postHandler(httpd_req_t *req);
    static httpd_uri_t getUri;
    static httpd_uri_t postUri;
    uint16_t port;
};
