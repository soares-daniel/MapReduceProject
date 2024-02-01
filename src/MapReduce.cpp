#include "MapReduce.h"
#include <stdexcept>
#include <iostream>

MapReduce::MapReduce(const KeyValueStore& store) : kvStore(store) {
    initOperations();
}

void MapReduce::initOperations() {
    mapFunctions["square"] = [](int x) { return x * x; };
    mapFunctions["double"] = [](int x) { return x * 2; };
    mapFunctions["triple"] = [](int x) { return x * 3; };

    reduceFunctions["sum"] = {{[](int x, int y) { return x + y; }}, 0};
    reduceFunctions["product"] = {{[](int x, int y) { return x * y; }}, 1};
}


std::map<std::string, int> MapReduce::performMapReduce(
    const std::string& mapOp,
    const std::string& reduceOp,
    const std::vector<std::string>& keys) {
    auto mapFunctionIt = mapFunctions.find(mapOp);
    if (mapFunctionIt == mapFunctions.end()) {
        throw std::runtime_error("Map operation not found: " + mapOp);
    }
    auto reduceFunctionIt = reduceFunctions.find(reduceOp);
    if (reduceFunctionIt == reduceFunctions.end()) {
        throw std::runtime_error("Reduce operation not found: " + reduceOp);
    }

    std::map<std::string, int> results;
    for (const auto& key : keys) {
        std::vector<int> values;
        try {
            values = kvStore.getValues(key);
        } catch (const std::runtime_error& e) {
            results[key] = reduceFunctionIt->second.second;
        }

        if (values.empty()) {
            // If there are no values to reduce, use the identity element for the reduce operation.
            results[key] = reduceFunctionIt->second.second;
            continue;
        }

        std::vector<int> mappedValues;
        for (int value : values) {
            mappedValues.push_back(mapFunctionIt->second(value));
        }

        int reducedValue = reduceFunctionIt->second.second; // Assume reduce operation is such that starting with 0 is appropriate
        auto reduceFunction = reduceFunctionIt->second.first;
        for (int mappedValue : mappedValues) {
            reducedValue = reduceFunction(reducedValue, mappedValue);
        }

        results[key] = reducedValue;
    }

    return results;
}