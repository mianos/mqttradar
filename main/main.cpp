// mqttradar — ESP32-C3 presence sensor on ESP-IDF v6.
//
// An LD1125 mmWave radar (UART) reports presence/tracking, published over MQTT.
// Settings live in NVS and are adjustable over MQTT (cmnd/<name>/settings) and
// HTTP (/config). A web server exposes /healthz, /config and OTA (/firmware)
// with rollback verification. Shared infrastructure (wifimanager, mqttwrapper,
// webserver, jsonwrapper, nvsstoragemanager) comes from the mianesp components;
// the radar driver comes from the ldradar component.

#include <cstring>
#include <ctime>
#include <regex>
#include <string>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "esp_sntp.h"
#include "esp_ota_ops.h"

#include "driver/gpio.h"
#include "driver/uart.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "LD2450.h"
#include "DebounceRadar.h"
#include "RadarSensor.h"
#include "Events.h"

#include "JsonWrapper.h"
#include "NvsStorageManager.h"
#include "Settings.h"
#include "WifiManager.h"
#include "MqttClient.h"
#include "MqttEventProc.h"
#include "WebServer.h"
#include "RadarWebServer.h"

static const char* kTag = "mqttradar";
static const char* kRadarTag = "ld2450_main";

// LD2450 radar on the XIAO ESP32-C3: silkscreen D6=GPIO21, D7=GPIO20, wired
// to UART1 (ESP TX=GPIO21->radar RX, ESP RX=GPIO20<-radar TX). Matches the
// pre-v6 Kconfig defaults LD2450_TX_PIN=21 / LD2450_RX_PIN=20. The LD2450
// streams at 256000 baud (see setupRadarUart).
static constexpr uart_port_t kUartPort = UART_NUM_1;
static constexpr gpio_num_t   kUartTx  = GPIO_NUM_21;
static constexpr gpio_num_t   kUartRx  = GPIO_NUM_20;

namespace {

struct App {
    Settings*    settings;
    MqttClient*  mqtt;
    WiFiManager* wifi;
};

std::string uptimeString() {
    uint32_t seconds = (uint32_t)(esp_timer_get_time() / 1000000ULL);
    uint32_t days = seconds / 86400; seconds %= 86400;
    uint32_t hours = seconds / 3600; seconds %= 3600;
    uint32_t minutes = seconds / 60;
    return std::to_string(days) + "d " + std::to_string(hours) + "h " +
           std::to_string(minutes) + "m";
}

std::string localIp() {
    char buf[16] = "0.0.0.0";
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip;
    if (netif && esp_netif_get_ip_info(netif, &ip) == ESP_OK) {
        esp_ip4addr_ntoa(&ip.ip, buf, sizeof(buf));
    }
    return std::string(buf);
}

// ---- MQTT command handlers (cmnd/<name>/<cmd>) ----

esp_err_t handleSettings(MqttClient* client, const std::string& topic,
                         const JsonWrapper& d, void* ctx) {
    auto* app = static_cast<App*>(ctx);
    auto changes = app->settings->loadFromJson(d);
    app->settings->save();
    app->settings->log();
    std::string ack = Settings::changesToJson(changes);
    client->publish("tele/" + app->settings->sensorName + "/settingsack", ack);
    ESP_LOGI(kTag, "applied settings for %s: %s", topic.c_str(), ack.c_str());
    return ESP_OK;
}

esp_err_t handleRestart(MqttClient*, const std::string&, const JsonWrapper&, void*) {
    ESP_LOGW(kTag, "restart requested");
    esp_restart();
    return ESP_OK;
}

esp_err_t handleReprovision(MqttClient*, const std::string&, const JsonWrapper&, void* ctx) {
    ESP_LOGW(kTag, "reprovision requested");
    static_cast<App*>(ctx)->wifi->clear();  // clears Wi-Fi creds and restarts
    return ESP_OK;
}

// ---- Radar event sinks ----

class PrintEP : public EventProc {
public:
    void Detected(Value* value_ptr) override {
        if (value_ptr == nullptr) return;
        JsonWrapper doc;
        doc.AddItem("event", std::string("detected"));
        value_ptr->toJson(doc);
        ESP_LOGI(kRadarTag, "%s", doc.ToString().c_str());
    }
    void Cleared() override {
        JsonWrapper doc;
        doc.AddItem("event", std::string("cleared"));
        ESP_LOGI(kRadarTag, "%s", doc.ToString().c_str());
    }
    void TrackingUpdate(Value* value_ptr) override {
        if (value_ptr == nullptr) return;
        const uint32_t interval_ms = static_cast<uint32_t>(CONFIG_LD1125_TRACKING_INTERVAL_MS);
        static uint32_t last_ms = 0;
        const uint32_t now_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000);
        if (interval_ms == 0 || now_ms - last_ms >= interval_ms) {
            JsonWrapper doc;
            doc.AddItem("event", std::string("tracking"));
            value_ptr->toJson(doc);
            ESP_LOGI(kRadarTag, "%s", doc.ToString().c_str());
            last_ms = now_ms;
        }
    }
    void PresenceUpdate(Value* value_ptr) override {
        const uint32_t interval_ms = static_cast<uint32_t>(CONFIG_LD1125_PRESENCE_INTERVAL_MS);
        static uint32_t last_ms = 0;
        const uint32_t now_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000);
        if (interval_ms == 0 || now_ms - last_ms >= interval_ms) {
            JsonWrapper doc;
            doc.AddItem("event", std::string("presence"));
            if (value_ptr) value_ptr->toJson(doc);
            ESP_LOGI(kRadarTag, "%s", doc.ToString().c_str());
            last_ms = now_ms;
        }
    }
};

// Fan an event out to both the console logger and the MQTT publisher.
class CombinedEP : public EventProc {
public:
    CombinedEP(EventProc* ep_a_in, EventProc* ep_b_in) : ep_a(ep_a_in), ep_b(ep_b_in) {}
    void Detected(Value* value_ptr) override {
        if (ep_a) ep_a->Detected(value_ptr);
        if (ep_b) ep_b->Detected(value_ptr);
    }
    void Cleared() override {
        if (ep_a) ep_a->Cleared();
        if (ep_b) ep_b->Cleared();
    }
    void TrackingUpdate(Value* value_ptr) override {
        if (ep_a) ep_a->TrackingUpdate(value_ptr);
        if (ep_b) ep_b->TrackingUpdate(value_ptr);
    }
    void PresenceUpdate(Value* value_ptr) override {
        if (ep_a) ep_a->PresenceUpdate(value_ptr);
        if (ep_b) ep_b->PresenceUpdate(value_ptr);
    }
private:
    EventProc* ep_a;
    EventProc* ep_b;
};

void radarTask(void* arg) {
    auto* combined = static_cast<CombinedEP*>(arg);
    LD2450 ld2450(combined, kUartPort);  // streams by default; no test-mode handshake
    DebounceRadar debounced(&ld2450, combined, 1000);
    for (;;) {
        debounced.process(0.0f);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void telemetryTask(void* arg) {
    auto* app = static_cast<App*>(arg);
    const std::string base = "tele/" + app->settings->sensorName + "/";

    // Wait (bounded) for SNTP so the init timestamp is real.
    for (int i = 0; i < 20 && time(nullptr) < 1700000000; ++i) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    JsonWrapper init;
    init.AddItem("version", 5);
    init.AddTime();
    init.AddTime(false, "gmt");
    init.AddItem("hostname", app->settings->sensorName);
    init.AddItem("ip", localIp());
    init.AddItem("settings", "cmnd/" + app->settings->sensorName + "/settings");
    app->mqtt->publish(base + "init", init.ToString());

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(60000));
        JsonWrapper d;
        d.AddTime();
        d.AddItem("uptime", uptimeString());
        d.AddItem("heap_free", (int)esp_get_free_heap_size());
        d.AddItem("heap_min_free", (int)esp_get_minimum_free_heap_size());
        app->mqtt->publish(base + "status", d.ToString());
    }
}

// --- OTA rollback verification ---
//
// A freshly-OTA'd image boots in PENDING_VERIFY: the bootloader rolls it back
// to the previous slot on the next reset unless the running app declares itself
// good. We declare it good only once the device is back on the network, so an
// image that boots but can't reach Wi-Fi is rolled back instead of stranding an
// unreachable device. A wired first-flash is UNDEFINED, so this never touches
// normal/bench boots.
constexpr int OTA_VERIFY_TIMEOUT_MS = 120000;
SemaphoreHandle_t s_got_ip = nullptr;

void onGotIp(void*, esp_event_base_t base, int32_t id, void*) {
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP && s_got_ip) {
        xSemaphoreGive(s_got_ip);
    }
}

void otaVerifyTask(void*) {
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    if (esp_ota_get_state_partition(running, &state) == ESP_OK &&
        state == ESP_OTA_IMG_PENDING_VERIFY) {
        ESP_LOGW(kTag, "OTA: image pending verify; waiting up to %ds for connectivity",
                 OTA_VERIFY_TIMEOUT_MS / 1000);
        if (xSemaphoreTake(s_got_ip, pdMS_TO_TICKS(OTA_VERIFY_TIMEOUT_MS)) == pdTRUE) {
            esp_ota_mark_app_valid_cancel_rollback();
            ESP_LOGI(kTag, "OTA: connectivity confirmed, image marked valid");
        } else {
            ESP_LOGE(kTag, "OTA: no IP within timeout; rolling back to previous image");
            esp_ota_mark_app_invalid_rollback_and_reboot();  // reboots on success
            ESP_LOGE(kTag, "OTA: rollback not possible; keeping current image");
            esp_ota_mark_app_valid_cancel_rollback();
        }
    }
    vTaskDelete(nullptr);
}

// Bring up UART1 for the LD2450 radar. Returns false on failure.
bool setupRadarUart() {
    uart_config_t uart_cfg;
    std::memset(&uart_cfg, 0, sizeof(uart_cfg));
    uart_cfg.baud_rate  = 256000;
    uart_cfg.data_bits  = UART_DATA_8_BITS;
    uart_cfg.parity     = UART_PARITY_DISABLE;
    uart_cfg.stop_bits  = UART_STOP_BITS_1;
    uart_cfg.flow_ctrl  = UART_HW_FLOWCTRL_DISABLE;
    uart_cfg.source_clk = UART_SCLK_DEFAULT;

    if (uart_param_config(kUartPort, &uart_cfg) != ESP_OK) {
        ESP_LOGE(kRadarTag, "uart_param_config failed");
        return false;
    }
    if (uart_set_pin(kUartPort, kUartTx, kUartRx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
        ESP_LOGE(kRadarTag, "uart_set_pin failed");
        return false;
    }
    if (uart_driver_install(kUartPort, 2048, 0, 0, nullptr, 0) != ESP_OK) {
        ESP_LOGE(kRadarTag, "uart_driver_install failed");
        return false;
    }
    return true;
}

}  // namespace

extern "C" void app_main(void) {
    // Silence info-level logs globally (keeps the per-event radar JSON prints
    // quiet) but keep network bring-up visible so Wi-Fi association and
    // provisioning state can be seen.
    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set("WiFiManager", ESP_LOG_INFO);
    esp_log_level_set("wifi", ESP_LOG_INFO);
    esp_log_level_set("esp_netif_handlers", ESP_LOG_INFO);  // prints "sta ip: ..."
    esp_log_level_set("MqttClient", ESP_LOG_INFO);
    esp_log_level_set("ld2450_main", ESP_LOG_INFO);         // decoded radar events

    static NvsStorageManager nvs;
    static Settings settings(nvs);
    settings.log();

    // Created before Wi-Fi starts so the got-IP handler can signal it.
    s_got_ip = xSemaphoreCreateBinary();

    // Wi-Fi: provisions (ESP-Touch v2) on first boot, else reconnects. onGotIp
    // feeds OTA rollback verification; publishes queue until MQTT connects.
    static WiFiManager wifi(nvs, onGotIp, nullptr);
    std::string host = settings.sensorName;
    wifi.configSetHostName(host);

    // NTP time in the configured timezone.
    setenv("TZ", settings.tz.c_str(), 1);
    tzset();
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, settings.ntpServer.c_str());
    esp_sntp_init();

    // MQTT (plain mqtt://, optional creds, Last-Will on the status topic).
    static std::string statusTopic = "tele/" + settings.sensorName + "/status";
    static std::string lwt = "{\"status\":\"offline\"}";
    static esp_mqtt_client_config_t mcfg = {};
    mcfg.broker.address.uri                  = settings.mqttBrokerUri.c_str();
    mcfg.credentials.client_id               = settings.sensorName.c_str();
    mcfg.credentials.username                = settings.mqttUserName.c_str();
    mcfg.credentials.authentication.password = settings.mqttUserPassword.c_str();
    mcfg.session.last_will.topic             = statusTopic.c_str();
    mcfg.session.last_will.msg               = lwt.c_str();
    mcfg.session.last_will.msg_len           = (int)lwt.size();
    mcfg.session.last_will.qos               = 1;
    static MqttClient mqtt(mcfg, settings.sensorName);

    static App app{ &settings, &mqtt, &wifi };

    const std::string b = "cmnd/" + settings.sensorName + "/";
    mqtt.registerHandler(b + "settings",    std::regex(b + "settings"),    handleSettings,    &app);
    mqtt.registerHandler(b + "restart",     std::regex(b + "restart"),     handleRestart,     &app);
    mqtt.registerHandler(b + "reprovision", std::regex(b + "reprovision"), handleReprovision, &app);
    mqtt.start();

    // Web server: base /healthz, /reset, /set_hostname plus /config and OTA.
    static WebContext webctx(&wifi);
    static RadarWebServer web(&webctx, settings);
    web.start();

    // OTA: verify a freshly-OTA'd image once it's back online; roll back if not.
    xTaskCreate(otaVerifyTask, "ota_verify", 4096, nullptr, 4, nullptr);

    // Radar -> console + MQTT.
    static PrintEP print_ep;
    static MqttEventProc mqtt_ep(settings, mqtt);
    static CombinedEP combined_ep(&print_ep, &mqtt_ep);

    if (!setupRadarUart()) {
        ESP_LOGE(kRadarTag, "radar UART init failed; radar disabled");
    } else {
        xTaskCreate(radarTask, "radar", 4096, &combined_ep, 5, nullptr);
    }

    xTaskCreate(telemetryTask, "telemetry", 4096, &app, 4, nullptr);

    ESP_LOGI(kTag, "mqttradar started");
}
