#include "KeyValueStore.h"
#include <stdexcept>

void KeyValueStore::insert(const std::string& key, int value) {
    if (store.find(key) == store.end()) {
        store[key] = std::vector<int>();
    }
    store[key].push_back(value);
}

void KeyValueStore::insertMany(const std::string& key, const std::vector<int>& values) {
    if (store.find(key) == store.end()) {
        store[key] = std::vector<int>();
    }
    store[key].insert(store[key].end(), values.begin(), values.end());
}

bool KeyValueStore::removeValue(const std::string& key, int value) {
    if (store.find(key) == store.end()) {
        return false;
    }
    std::vector<int>& values = store[key];
    for (auto it = values.begin(); it != values.end(); ++it) {
        if (*it == value) {
            values.erase(it);
            return true;
        }
    }
    return false;
}

bool KeyValueStore::removeKey(const std::string& key) {
    return store.erase(key) > 0;
}

bool KeyValueStore::removeMany(const std::string& key, const std::vector<int>& values) {
    if (store.find(key) == store.end()) {
        return false;
    }
    std::vector<int>& storeValues = store[key];
    for (int value : values) {
        for (auto it = storeValues.begin(); it != storeValues.end(); ++it) {
            if (*it == value) {
                storeValues.erase(it);
                break;
            }
        }
    }
    return true;
}

std::vector<int> KeyValueStore::getValues(const std::string& key) const {
    if (store.find(key) == store.end()) {
        throw std::runtime_error("Key " + key + " not found");
    }
    return store.at(key);
}

const std::map<std::string, std::vector<int>>& KeyValueStore::getAll() const {
    return store;
}

const KeyValueStore KeyValueStore::getCopy() const {
    KeyValueStore copy;
    for (const auto& pair : store) {
        copy.insertMany(pair.first, pair.second);
    }
    return copy;
}