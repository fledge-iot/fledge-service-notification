#include <gtest/gtest.h>
#include "notification_manager.h"
#include "reading.h"
#include "reading_set.h"
#include "datapoint.h"

using namespace std;

class FilterPipelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create notification instance for testing
        NOTIFICATION_TYPE nType;
        nType.type = E_NOTIFICATION_TYPE::OneShot;
        nType.retriggerTimeTv.tv_sec = 60;
        
        instance = new NotificationInstance(
            "test_notification",
            true,
            nType,
            nullptr,
            nullptr
        );
    }
    
    void TearDown() override {
        delete instance;
        instance = nullptr;
    }
    
    NotificationInstance* instance;
};

// Test 1: Basic filter pipeline lifecycle
TEST_F(FilterPipelineTest, PipelineLifecycle) {
    // Test initial state
    EXPECT_FALSE(instance->hasFilterPipeline());
    EXPECT_FALSE(instance->hasActiveFilters());
    
    // Test cleanup when no pipeline exists (should not crash)
    instance->cleanupFilterPipeline();
    EXPECT_FALSE(instance->hasFilterPipeline());
    
    // Test hasActiveFilters when no pipeline exists
    EXPECT_FALSE(instance->hasActiveFilters());
}

// Test 2: Error handling
TEST_F(FilterPipelineTest, ErrorHandling) {
    // Test that methods handle null/empty cases gracefully
    EXPECT_FALSE(instance->hasFilterPipeline());
    EXPECT_FALSE(instance->hasActiveFilters());
    
    // Test processDataThroughFilter with null readings
    EXPECT_FALSE(instance->processDataThroughFilter(nullptr));
    
    // Test processDataThroughFilter with empty readings
    ReadingSet* emptyReadings = new ReadingSet();
    EXPECT_FALSE(instance->processDataThroughFilter(emptyReadings));
    delete emptyReadings;
}

// Test 3: Filtered data management
TEST_F(FilterPipelineTest, FilteredDataManagement) {
    // Create test readings
    ReadingSet* readings = new ReadingSet();
    DatapointValue dv(42.0);
    Reading* reading = new Reading("test_asset", new Datapoint("test_datapoint", dv));
    std::vector<Reading*> readingVector;
    readingVector.push_back(reading);
    readings->append(readingVector);
    
    // Test setter and getter
    instance->setFilteredData(readings);
    EXPECT_EQ(instance->getFilteredData(), readings);
    
    // Test clear functionality
    instance->clearFilteredData();
    EXPECT_EQ(instance->getFilteredData(), nullptr);
    
    // Test with null data
    instance->setFilteredData(nullptr);
    EXPECT_EQ(instance->getFilteredData(), nullptr);
}

// Test 4: Data processing
TEST_F(FilterPipelineTest, DataProcessing) {
    // Test with null readings
    EXPECT_FALSE(instance->processDataThroughFilter(nullptr));
    
    // Test with empty readings
    ReadingSet* emptyReadings = new ReadingSet();
    EXPECT_FALSE(instance->processDataThroughFilter(emptyReadings));
    delete emptyReadings;
    
    // Test with valid readings (no pipeline configured)
    ReadingSet* readings = new ReadingSet();
    DatapointValue dv(42.0);
    Reading* reading = new Reading("test_asset", new Datapoint("test_datapoint", dv));
    std::vector<Reading*> readingVector;
    readingVector.push_back(reading);
    readings->append(readingVector);
    EXPECT_FALSE(instance->processDataThroughFilter(readings)); // Should return false when no pipeline
    delete readings;
}

// Test 5: Callback functions
TEST_F(FilterPipelineTest, CallbackFunctions) {
    // Test with null parameters (should not crash)
    passToOnwardFilter(nullptr, nullptr);
    receiveFilteredData(nullptr, nullptr);
    
    // Test with valid parameters
    ReadingSet* readings = new ReadingSet();
    DatapointValue dv(42.0);
    Reading* reading = new Reading("test_asset", new Datapoint("test_datapoint", dv));
    std::vector<Reading*> readingVector;
    readingVector.push_back(reading);
    readings->append(readingVector);
    
    receiveFilteredData(static_cast<OUTPUT_HANDLE*>(instance), readings);
    EXPECT_EQ(instance->getFilteredData(), readings);
    
    instance->clearFilteredData();
}

// Test 6: Multiple instances
TEST_F(FilterPipelineTest, MultipleInstances) {
    NOTIFICATION_TYPE nType;
    nType.type = E_NOTIFICATION_TYPE::OneShot;
    nType.retriggerTimeTv.tv_sec = 60;
    
    NotificationInstance instance1("test1", true, nType, nullptr, nullptr);
    NotificationInstance instance2("test2", true, nType, nullptr, nullptr);
    
    // Test independent operation
    ReadingSet* readings1 = new ReadingSet();
    DatapointValue dv1(10.0);
    Reading* reading1 = new Reading("asset1", new Datapoint("datapoint1", dv1));
    std::vector<Reading*> readingVector1;
    readingVector1.push_back(reading1);
    readings1->append(readingVector1);
    
    ReadingSet* readings2 = new ReadingSet();
    DatapointValue dv2(20.0);
    Reading* reading2 = new Reading("asset2", new Datapoint("datapoint2", dv2));
    std::vector<Reading*> readingVector2;
    readingVector2.push_back(reading2);
    readings2->append(readingVector2);
    
    instance1.setFilteredData(readings1);
    instance2.setFilteredData(readings2);
    
    EXPECT_EQ(instance1.getFilteredData(), readings1);
    EXPECT_EQ(instance2.getFilteredData(), readings2);
    
    // Cleanup
    instance1.clearFilteredData();
    instance2.clearFilteredData();
    //delete readings1;
    //delete readings2;
}

// Test 7: Memory management
TEST_F(FilterPipelineTest, MemoryManagement) {
    // Test multiple set/clear cycles
    for (int i = 0; i < 5; i++) {
        ReadingSet* readings = new ReadingSet();
        DatapointValue dv((long)i);
        Reading* reading = new Reading("asset", new Datapoint("datapoint", dv));
        std::vector<Reading*> readingVector;
        readingVector.push_back(reading);
        readings->append(readingVector);
        
        instance->setFilteredData(readings);
        instance->clearFilteredData();
        
        EXPECT_EQ(instance->getFilteredData(), nullptr);
    }
}

// Test 8: Edge cases
TEST_F(FilterPipelineTest, EdgeCases) {
    // Test with empty reading set
    ReadingSet* emptyReadings = new ReadingSet();
    instance->setFilteredData(emptyReadings);
    EXPECT_EQ(instance->getFilteredData(), emptyReadings);
    instance->clearFilteredData();
    
    // Test clear when already null
    instance->clearFilteredData();
    EXPECT_EQ(instance->getFilteredData(), nullptr);
    
    // Test getFilteredData when null
    EXPECT_EQ(instance->getFilteredData(), nullptr);
}

// Test 9: Performance stress test
TEST_F(FilterPipelineTest, PerformanceStress) {
    // Test rapid data operations
    for (int i = 0; i < 100; i++) {
        ReadingSet* readings = new ReadingSet();
        DatapointValue dv((long)i);
        Reading* reading = new Reading("asset", new Datapoint("datapoint", dv));
        std::vector<Reading*> readingVector;
        readingVector.push_back(reading);
        readings->append(readingVector);
        
        instance->setFilteredData(readings);
        instance->getFilteredData();
        instance->clearFilteredData();
    }
    
    EXPECT_EQ(instance->getFilteredData(), nullptr);
}

// Test 10: Thread safety (basic)
TEST_F(FilterPipelineTest, ThreadSafety) {
    ReadingSet* readings = new ReadingSet();
    DatapointValue dv(42.0);
    Reading* reading = new Reading("test_asset", new Datapoint("test_datapoint", dv));
    std::vector<Reading*> readingVector;
    readingVector.push_back(reading);
    readings->append(readingVector);
    
    // Basic thread safety test
    std::thread t1([this, readings]() {
        instance->setFilteredData(readings);
    });
    
    std::thread t2([this]() {
        instance->getFilteredData();
    });
    
    t1.join();
    t2.join();
    
    instance->clearFilteredData();
}

