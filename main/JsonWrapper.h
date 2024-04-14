#pragma once
#include <cJSON.h>
#include <memory>
#include <string>

class JsonWrapper {
public:
    // Prevents implicit copying to avoid double-free errors
    JsonWrapper(const JsonWrapper&) = delete;
    JsonWrapper& operator=(const JsonWrapper&) = delete;

    // Enable move semantics to transfer ownership safely
    JsonWrapper(JsonWrapper&&) noexcept = default;
    JsonWrapper& operator=(JsonWrapper&&) noexcept = default;

    // Constructor takes ownership of cJSON*.
    explicit JsonWrapper(cJSON* jsonObj = nullptr) noexcept 
        : jsonObj_(jsonObj, [](cJSON* ptr) { if (ptr) cJSON_Delete(ptr); }) {}

    // Parse JSON string into JsonWrapper.
    static JsonWrapper Parse(const std::string& rawData) {
        return JsonWrapper(cJSON_Parse(rawData.c_str()));
    }

    // Serialize JsonWrapper back to string.
    std::string ToString() const {
        std::unique_ptr<char, decltype(&cJSON_free)> rawJsonString(cJSON_PrintUnformatted(jsonObj_.get()), &cJSON_free);
        return rawJsonString ? std::string(rawJsonString.get()) : std::string();
    }

    // Explicitly release the cJSON object.
    cJSON* Release() noexcept {
        return jsonObj_.release();
    }

    // Check if the wrapper is empty.
    bool Empty() const noexcept {
        return jsonObj_ == nullptr;
    }

	void AddItem(const std::string& key, double value) {
		if (!jsonObj_) jsonObj_.reset(cJSON_CreateObject());
		cJSON_AddNumberToObject(jsonObj_.get(), key.c_str(), value);
	}

	void AddItem(const std::string& key, int value) {
		if (!jsonObj_) jsonObj_.reset(cJSON_CreateObject());
		cJSON_AddNumberToObject(jsonObj_.get(), key.c_str(), value);
	}

	void AddItem(const std::string& key, const std::string& value) {
		if (!jsonObj_) jsonObj_.reset(cJSON_CreateObject());
		cJSON_AddStringToObject(jsonObj_.get(), key.c_str(), value.c_str());
	}

	void AddTime(bool local=true, const char *field="time") {
		time_t now = time(NULL);
		struct tm timeinfo;
		char time_string[32];	// max should be 24 for local time.

		if (local) {
			localtime_r(&now, &timeinfo);
			strftime(time_string, sizeof(time_string), "%FT%T%z", &timeinfo);
		} else {
			gmtime_r(&now, &timeinfo);
			strftime(time_string, sizeof(time_string), "%FT%TZ", &timeinfo);
		}
		strftime(time_string, sizeof(time_string), "%FT%T", &timeinfo);
		if (!jsonObj_) jsonObj_.reset(cJSON_CreateObject());
		cJSON_AddStringToObject(jsonObj_.get(), field, time_string);
	}


private:
    std::unique_ptr<cJSON, decltype(&cJSON_Delete)> jsonObj_;
};

