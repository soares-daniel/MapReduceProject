#pragma once

#include <map>
#include <vector>
#include <string>

class KeyValueStore {
public:
    void insert(const std::string& key, int value);
    void insertMany(const std::string& key, const std::vector<int>& values);
    bool removeValue(const std::string& key, int value);
    bool removeMany(const std::string& key, const std::vector<int>& values);
    bool removeKey(const std::string& key);
    std::vector<int> getValues(const std::string& key) const;
    const std::map<std::string, std::vector<int>>& getAll() const;
    const KeyValueStore getCopy() const;
private:
    std::map<std::string, std::vector<int>> store;
};
