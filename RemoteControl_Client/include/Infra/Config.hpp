#pragma once

#include <string>
#include <unordered_map>
#include <fstream>
#include <sstream>

class ConfigManager {
public:
    static ConfigManager& getInstance() {
        static ConfigManager instance;
        return instance;
    }

    void loadConfig(const std::string& filepath) {
       std::ifstream file(filepath);
        std::string line;
        while (std::getline(file, line)) {
            auto delimiterPos = line.find('=');
            auto key = line.substr(0, delimiterPos);
            auto value = line.substr(delimiterPos + 1);
            config_[key] = value;
        }
    }

    std::string get(const std::string& key, const std::string& defaultValue = "") const {
        auto it = config_.find(key);
        return it != config_.end() ? it->second : defaultValue;
    }

private:
    ConfigManager() = default;
    ~ConfigManager() = default;

    std::unordered_map<std::string, std::string> config_;
};