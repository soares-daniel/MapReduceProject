#include <gtest/gtest.h>
#include "MapReduce.h"
#include "KeyValueStore.h"

class MapReduceTest : public ::testing::Test {
protected:
    KeyValueStore kvStore;
    MapReduce* mapReduce; // Change to pointer to dynamically initialize it in SetUp()

    MapReduceTest() : mapReduce(nullptr) {} // Ensure mapReduce is initialized to nullptr

    void SetUp() override {
        // Set up some data in the KeyValueStore
        kvStore.insert("Category1", 10);
        kvStore.insert("Category1", 20);
        kvStore.insert("Category2", 30);
        kvStore.insert("MixedCategory", 1);
        kvStore.insert("MixedCategory", 2);
        kvStore.insert("MixedCategory", -3);

        // Now initialize mapReduce after kvStore has been populated
        mapReduce = new MapReduce(kvStore);
    }

    void TearDown() override {
        // Clean up after each test case
        delete mapReduce;
        mapReduce = nullptr;
    }
};

// Test the basic functionality of map and reduce operations
TEST_F(MapReduceTest, BasicFunctionality) {
    std::vector<std::string> keys = {"Category1"};
    std::cout << "kvStore in MapReduce: " << std::endl;
    auto results = mapReduce->performMapReduce("double", "sum", keys);
    std::cout << "Results: " << std::endl;
    for (const auto& result : results) {
        std::cout << result.first << ": " << result.second << std::endl;
    }
    ASSERT_EQ(results["Category1"], 60); // (10*2 + 20*2)
}

// Test multiple keys
TEST_F(MapReduceTest, MultipleKeys) {
    std::vector<std::string> keys = {"Category1", "Category2"};
    auto results = mapReduce->performMapReduce("double", "sum", keys);
    ASSERT_EQ(results["Category1"], 60); // (10*2 + 20*2)
    ASSERT_EQ(results["Category2"], 60); // 30*2
}

// Test with non-existent map operation
TEST_F(MapReduceTest, NonExistentMapOperation) {
    std::vector<std::string> keys = {"Category1"};
    EXPECT_THROW(mapReduce->performMapReduce("nonExistentOp", "sum", keys), std::runtime_error);
}

// Test with non-existent reduce operation
TEST_F(MapReduceTest, NonExistentReduceOperation) {
    std::vector<std::string> keys = {"Category1"};
    EXPECT_THROW(mapReduce->performMapReduce("double", "nonExistentOp", keys), std::runtime_error);
}

// Test with non-existent key
TEST_F(MapReduceTest, NonExistentKey) {
    std::vector<std::string> keys = {"NonExistentKey"};
    auto results = mapReduce->performMapReduce("double", "sum", keys);
    ASSERT_TRUE(results["NonExistentKey"] == 0); // Expect the identity element for sum: 0
}

// Test with empty keys list
TEST_F(MapReduceTest, EmptyKeys) {
    std::vector<std::string> keys = {};
    auto results = mapReduce->performMapReduce("double", "sum", keys);
    ASSERT_TRUE(results.empty());
}

// Test the product reduce functionality with a single key
TEST_F(MapReduceTest, ProductReduceSingleKey) {
    std::vector<std::string> keys = {"Category1"};
    auto results = mapReduce->performMapReduce("double", "product", keys);
    ASSERT_EQ(results["Category1"], 800); // (10*2) * (20*2)
}

// Test the product reduce functionality with multiple keys
TEST_F(MapReduceTest, ProductReduceMultipleKeys) {
    std::vector<std::string> keys = {"Category1", "Category2"};
    auto results = mapReduce->performMapReduce("double", "product", keys);
    ASSERT_EQ(results["Category1"], 800); // (10*2) * (20*2)
    ASSERT_EQ(results["Category2"], 60); // (30*2) * (30*2)
}

// Test the product reduce with a 'square' map operation
TEST_F(MapReduceTest, SquareMapWithProductReduce) {
    std::vector<std::string> keys = {"Category1"};
    auto results = mapReduce->performMapReduce("square", "product", keys);
    ASSERT_EQ(results["Category1"], 40000); // (10^2) * (20^2)
}

// Test the product reduce with a 'triple' map operation
TEST_F(MapReduceTest, TripleMapWithProductReduce) {
    std::vector<std::string> keys = {"Category1"};
    auto results = mapReduce->performMapReduce("triple", "product", keys);
    ASSERT_EQ(results["Category1"], 1800); // (10*3) * (20*3)
}

// Test the product reduce with no values for a key (should result in the identity element for product: 1)
TEST_F(MapReduceTest, ProductReduceNoValues) {
    std::vector<std::string> keys = {"EmptyCategory"};
    auto results = mapReduce->performMapReduce("double", "product", keys);
    ASSERT_EQ(results["EmptyCategory"], 1); // No values, so the product should be the identity element: 1
}


// Test the product reduce with an empty vector (should return an empty map)
TEST_F(MapReduceTest, ProductReduceEmptyVector) {
    std::vector<std::string> keys;
    auto results = mapReduce->performMapReduce("triple", "product", keys);
    ASSERT_TRUE(results.empty());
}

// Add a test for product reduce with a mix of positive and negative values
TEST_F(MapReduceTest, ProductReduceMixedValues) {
    kvStore.insert("MixedCategory", -1);
    kvStore.insert("MixedCategory", 2);
    kvStore.insert("MixedCategory", -3);
    std::vector<std::string> keys = {"MixedCategory"};
    auto results = mapReduce->performMapReduce("double", "product", keys);
    ASSERT_EQ(results["MixedCategory"], -48); // (-1*2) * (2*2) * (-3*2)
}

// Test the product reduce with only one value for a key
TEST_F(MapReduceTest, ProductReduceSingleValue) {
    std::vector<std::string> keys = {"Category2"};
    auto results = mapReduce->performMapReduce("double", "product", keys);
    ASSERT_EQ(results["Category2"], 60); // 30*2
}
