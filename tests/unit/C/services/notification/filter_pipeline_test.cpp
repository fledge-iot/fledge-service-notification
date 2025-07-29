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
TEST_F(FilterPipelineTest, FilteredDataManagement) 
{
    // Create test readings
    ReadingSet* readings = new ReadingSet();
    DatapointValue dv(42.0);
    Reading* reading = new Reading("test_asset", new Datapoint("test_datapoint", dv));
    std::vector<Reading*> readingVector;
    readingVector.push_back(reading);
    readings->append(readingVector);
    
    // Test setter and getter
    instance->setFilteredData(readings);
    EXPECT_EQ(instance->acquireFilteredData(), readings);
       
    // Test with null data
    instance->setFilteredData(nullptr);
    EXPECT_EQ(instance->acquireFilteredData(), nullptr);

    delete readings; // Clean up after test
}

// Test 4: Data processing
TEST_F(FilterPipelineTest, DataProcessing) 
{
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
    delete readings; // Clean up after test
}

// Test 5: Callback functions
TEST_F(FilterPipelineTest, CallbackFunctions) 
{   
    // Test with valid parameters
    ReadingSet* readings = new ReadingSet();
    DatapointValue dv(42.0);
    Reading* reading = new Reading("test_asset", new Datapoint("test_datapoint", dv));
    std::vector<Reading*> readingVector;
    readingVector.push_back(reading);
    readings->append(readingVector);
    
    NotificationInstance::receiveFilteredData(static_cast<OUTPUT_HANDLE*>(instance), readings);
    EXPECT_EQ(instance->acquireFilteredData(), readings);
    delete readings; // Clean up after test
}

// Test 6: Multiple instances
TEST_F(FilterPipelineTest, MultipleInstances) 
{
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
    
    EXPECT_EQ(instance1.acquireFilteredData(), readings1);
    EXPECT_EQ(instance2.acquireFilteredData(), readings2);
    
    delete readings1; // Clean up after test
    delete readings2; // Clean up after test
}
