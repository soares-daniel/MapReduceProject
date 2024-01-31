#include <gtest/gtest.h>
#include "KeyValueStore.h"

class KeyValueStoreTest : public ::testing::Test {
protected:
    KeyValueStore kvStore;
};

// Test insertion and retrieval of values
TEST_F(KeyValueStoreTest, InsertAndRetrieveValues) {
    kvStore.insert("Books", 100);
    kvStore.insert("Books", 200);
    auto values = kvStore.getValues("Books");
    ASSERT_EQ(values.size(), 2);
    ASSERT_EQ(values[0], 100);
    ASSERT_EQ(values[1], 200);
}

// Test retrieval of values for a non-existent key
TEST_F(KeyValueStoreTest, RetrieveValuesNonExistentKey) {
    EXPECT_THROW(kvStore.getValues("NonExistentKey"), std::runtime_error);
}

// Test removing a specific value
TEST_F(KeyValueStoreTest, RemoveValue) {
    kvStore.insert("Books", 100);
    kvStore.insert("Books", 200);
    bool removed = kvStore.removeValue("Books", 100);
    ASSERT_TRUE(removed);
    auto values = kvStore.getValues("Books");
    ASSERT_EQ(values.size(), 1);
    ASSERT_EQ(values[0], 200);
}

// Test removing a value that does not exist
TEST_F(KeyValueStoreTest, RemoveNonExistentValue) {
    kvStore.insert("Books", 100);
    bool removed = kvStore.removeValue("Books", 300);
    ASSERT_FALSE(removed);
}

// Test removing a key
TEST_F(KeyValueStoreTest, RemoveKey) {
    kvStore.insert("Books", 100);
    bool removed = kvStore.removeKey("Books");
    ASSERT_TRUE(removed);
    EXPECT_THROW(kvStore.getValues("Books"), std::runtime_error);
}

// Test removing a non-existent key
TEST_F(KeyValueStoreTest, RemoveNonExistentKey) {
    bool removed = kvStore.removeKey("NonExistentKey");
    ASSERT_FALSE(removed);
}

// Test retrieving all key-value pairs
TEST_F(KeyValueStoreTest, GetAllKeyValuePairs) {
    kvStore.insert("Books", 100);
    kvStore.insert("Electronics", 200);
    auto allPairs = kvStore.getAll();
    ASSERT_EQ(allPairs.size(), 2);
    ASSERT_EQ(allPairs["Books"].size(), 1);
    ASSERT_EQ(allPairs["Electronics"].size(), 1);
    ASSERT_EQ(allPairs["Books"][0], 100);
    ASSERT_EQ(allPairs["Electronics"][0], 200);
}

// Test copying a KeyValueStore
TEST_F(KeyValueStoreTest, CopyKeyValueStore) {
    kvStore.insert("Books", 100);
    kvStore.insert("Electronics", 200);
    auto copy = kvStore.getCopy();
    kvStore.insert("Books", 300);
    ASSERT_EQ(copy.getValues("Books").size(), 1);
    ASSERT_EQ(copy.getValues("Electronics").size(), 1);
    ASSERT_EQ(copy.getValues("Books")[0], 100);
    ASSERT_EQ(copy.getValues("Electronics")[0], 200);
}
