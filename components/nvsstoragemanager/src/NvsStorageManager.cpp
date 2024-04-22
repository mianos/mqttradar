#include "nvs_flash.h"
#include "esp_log.h"
#include <cstring> // For memset and other string operations

#include "NvsStorageManager.h"

static const char *TAG = "NvsStorageManager";

NvsStorageManager::NvsStorageManager(const std::string& storageNamespace) : ns(storageNamespace) {
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK && err != ESP_ERR_NVS_NO_FREE_PAGES && err != ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(err));
        initialized = false;
    } else {
        initialized = true;
    }
}

bool NvsStorageManager::store(const std::string& key, const std::string& value) {
    if (!initialized) return false;

	if (key.size() > 15) {
        ESP_LOGE(TAG, "key longer than 15 chars '%s'", key.c_str());
	}
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(ns.c_str(), NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return false;
    }

    if ((err = nvs_set_str(nvs_handle, key.c_str(), value.c_str())) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to store '%s' value '%s' err is '%s'", key.c_str(), value.c_str(), esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }

    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return err == ESP_OK;
}

bool NvsStorageManager::retrieve(const std::string& key, std::string& value) const {
    if (!initialized)
		return false;

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(ns.c_str(), NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) return false;

    // Initialize buffer to a large size to accommodate most value sizes
    char value_buff[512];
    size_t value_size = sizeof(value_buff);
    memset(value_buff, 0, value_size);

    err = nvs_get_str(nvs_handle, key.c_str(), value_buff, &value_size);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
		ESP_LOGI(TAG, "Did not get NVS value '%s'", key.c_str());
        return false;
    }

    value = std::string(value_buff);
    nvs_close(nvs_handle);
    return true;
}

bool NvsStorageManager::clear(const std::string& key) {
    if (!initialized) return false;

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(ns.c_str(), NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) return false;

    err = nvs_erase_key(nvs_handle, key.c_str());
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        return false;
    }

    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return err == ESP_OK;
}

bool NvsStorageManager::clearAll() {
    if (!initialized) return false;

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(ns.c_str(), NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) return false;

    err = nvs_erase_all(nvs_handle);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        return false;
    }

    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return err == ESP_OK;
}
