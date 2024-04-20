#pragma once

#include <string>
#include <vector>
#include <utility> // For std::pair

class NvsStorageManager {
public:
    explicit NvsStorageManager(const std::string& storageNamespace = "storage");
    bool store(const std::string& key, const std::string& value);
    bool retrieve(const std::string& key, std::string& value) const;
    bool clear(const std::string& key);
    bool clearAll();

private:
    std::string ns;
    bool initialized;
};
