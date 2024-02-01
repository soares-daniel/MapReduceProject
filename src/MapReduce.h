#pragma once


#include "KeyValueStore.h" // Include your KeyValueStore header
#include <functional>
#include <map>
#include <string>
#include <vector>

class MapReduce {
public:
    MapReduce(const KeyValueStore& kvStore);
    std::map<std::string, int> performMapReduce(
        const std::string& mapOp,
        const std::string& reduceOp,
        const std::vector<std::string>& keys = std::vector<std::string>());

private:
    KeyValueStore kvStore;
    std::map<std::string, std::function<int(int)>> mapFunctions;
    std::map<std::string, std::pair<std::function<int(int, int)>, int>> reduceFunctions;

    void initOperations();
};
