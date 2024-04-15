#pragma once
#include <cJSON.h>
#include <memory>
#include <string.h>
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

    // Explicitly release the cJSON object.
    cJSON* Release() noexcept {
        return jsonObj_.release();
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


    std::string ToString() const {
        if (jsonObj_ == nullptr) {
            return "{}"; // Safe return for empty JSON object
        }
        char* jsonString = cJSON_PrintUnformatted(jsonObj_.get());
        if (!jsonString) {
            return "{}";
        }
        std::string result(jsonString);
        free(jsonString); // Ensure to free the allocated cJSON string
        return result;
    }

	void AddItem(const std::string& key, const std::string& value) {
		if (!jsonObj_) {
			jsonObj_.reset(cJSON_CreateObject());
		}
		cJSON_AddStringToObject(jsonObj_.get(), key.c_str(), value.c_str());
	}

    void AddItem(const std::string& key, int value) {
        if (!jsonObj_) {
            jsonObj_.reset(cJSON_CreateObject());
        }
        cJSON_AddNumberToObject(jsonObj_.get(), key.c_str(), value);
    }

    bool Empty() const noexcept {
        return jsonObj_ == nullptr || cJSON_GetArraySize(jsonObj_.get()) == 0;
    }

    template<typename T>
    bool GetField(const std::string& key, T& value, bool mandatory = false) const {
        if (!jsonObj_) {
            return false;  // JSON object not initialized
        }

        cJSON* item = cJSON_GetObjectItem(jsonObj_.get(), key.c_str());
        if (!item) {
            return !mandatory;  // Return false if the field is mandatory, true otherwise
        }

        return assignValue(item, value);
    }

    bool ContainsField(const std::string& key) const {
        if (!jsonObj_) return false;
        cJSON* item = cJSON_GetObjectItem(jsonObj_.get(), key.c_str());
        return item != nullptr;
    }

private:
    template<typename T>
    bool assignValue(cJSON* item, T& value) const {
        if constexpr (std::is_same_v<T, std::string>) {
            if (cJSON_IsString(item)) {
                value = item->valuestring;
                return true;
            }
        } else if constexpr (std::is_integral_v<T> || std::is_floating_point_v<T>) {
            if (cJSON_IsNumber(item)) {
                value = static_cast<T>(item->valuedouble);  // Use static_cast appropriately
                return true;
            }
        }
        return false;  // Type mismatch or unable to convert
    }
    std::unique_ptr<cJSON, decltype(&cJSON_Delete)> jsonObj_;
};

