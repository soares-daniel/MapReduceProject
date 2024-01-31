#include <gtest/gtest.h>
#include "MapReduce.h"
#include "KeyValueStore.h"

class MapReduceTest : public ::testing::Test {
protected:
    KeyValueStore kvStore;
    MapReduce mapReduce;

    MapReduceTest() : mapReduce(kvStore) {}

    void SetUp() override {
        // Set up some data in the KeyValueStore
        kvStore.insert("Category1", 10);
        kvStore.insert("Category1", 20);
        kvStore.insert("Category2", 30);
    }
};

// Test the basic functionality of map and reduce operations
TEST_F(MapReduceTest, BasicFunctionality) {
    std::vector<std::string> keys = {"Category1"};
    auto results = mapReduce.performMapReduce("double", "sum", keys);
    ASSERT_EQ(results["Category1"], 60); // (10*2 + 20*2)
}

// Test multiple keys
TEST_F(MapReduceTest, MultipleKeys) {
    std::vector<std::string> keys = {"Category1", "Category2"};
    auto results = mapReduce.performMapReduce("double", "sum", keys);
    ASSERT_EQ(results["Category1"], 60); // (10*2 + 20*2)
    ASSERT_EQ(results["Category2"], 60); // 30*2
}

// Test with non-existent map operation
TEST_F(MapReduceTest, NonExistentMapOperation) {
    std::vector<std::string> keys = {"Category1"};
    EXPECT_THROW(mapReduce.performMapReduce("nonExistentOp", "sum", keys), std::runtime_error);
}

// Test with non-existent reduce operation
TEST_F(MapReduceTest, NonExistentReduceOperation) {
    std::vector<std::string> keys = {"Category1"};
    EXPECT_THROW(mapReduce.performMapReduce("double", "nonExistentOp", keys), std::runtime_error);
}

// Test with non-existent key
TEST_F(MapReduceTest, NonExistentKey) {
    std::vector<std::string> keys = {"NonExistentKey"};
    auto results = mapReduce.performMapReduce("double", "sum", keys);
    ASSERT_TRUE(results.find("NonExistentKey") == results.end());
}

// Test with empty keys list
TEST_F(MapReduceTest, EmptyKeys) {
    std::vector<std::string> keys = {};
    auto results = mapReduce.performMapReduce("double", "sum", keys);
    ASSERT_TRUE(results.empty());
}

