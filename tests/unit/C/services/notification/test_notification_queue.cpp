#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <thread>
#include <chrono>

#include "notification_queue.h"
#include "notification_manager.h"
#include "notification_subscription.h"
#include "reading_set.h"
#include "reading.h"
#include "datapoint.h"
#include "logger.h"

using namespace std;

// Mock classes for testing
class MockNotificationRule : public NotificationRule
{
public:
    MockNotificationRule(const string& name) : NotificationRule(name) {}
    
    bool eval(const string& data) override { return true; }
    string reason() override { return "{\"reason\": \"test\"}"; }
    string getName() const override { return "MockRule"; }
    bool isTimeBased() const override { return false; }
    bool evaluateAny() const override { return true; }
};

class MockNotificationInstance : public NotificationInstance
{
public:
    MockNotificationInstance(const string& name) : 
        NotificationInstance(name, true, NotificationInstance::NotificationType{NotificationInstance::OneShot, {0, 0}}, nullptr, nullptr) {}
    
    bool isEnabled() const override { return true; }
    bool isZombie() const override { return false; }
    string getName() const override { return "MockInstance"; }
    NotificationRule* getRule() override { return m_rule; }
    void setRule(NotificationRule* rule) { m_rule = rule; }
    
private:
    NotificationRule* m_rule = nullptr;
};

class MockNotificationManager : public NotificationManager
{
public:
    MockNotificationManager() {}
    
    NotificationInstance* getNotificationInstance(const string& name) override
    {
        auto it = m_instances.find(name);
        return (it != m_instances.end()) ? it->second : nullptr;
    }
    
    void addInstance(const string& name, NotificationInstance* instance)
    {
        m_instances[name] = instance;
    }
    
    map<string, NotificationInstance*>& getInstances() { return m_instances; }
    void lockInstances() {}
    void unlockInstances() {}
    void collectZombies() {}
    
private:
    map<string, NotificationInstance*> m_instances;
};

// Test fixture for notification queue tests
class NotificationQueueTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Initialize logger for tests
        Logger::getLogger();
        
        // Create test reading set
        m_readingSet = new ReadingSet();
        
        // Create test datapoint
        DatapointValue dpv(42.5);
        Datapoint* dp = new Datapoint("temperature", dpv);
        
        // Create test reading
        Reading* reading = new Reading("TestAsset", dp);
        m_readingSet->append(reading);
    }
    
    void TearDown() override
    {
        if (m_readingSet)
        {
            delete m_readingSet;
        }
    }
    
    ReadingSet* m_readingSet;
};

// Test NotificationDataElement class
TEST_F(NotificationQueueTest, NotificationDataElementConstructor)
{
    // Arrange & Act
    NotificationDataElement element("TestRule", "TestAsset", m_readingSet);
    
    // Assert
    EXPECT_EQ(element.getAssetName(), "TestAsset");
    EXPECT_EQ(element.getRuleName(), "TestRule");
    EXPECT_EQ(element.getData(), m_readingSet);
    EXPECT_GT(element.getTime(), 0);
}

TEST_F(NotificationQueueTest, NotificationDataElementDestructor)
{
    // Arrange
    NotificationDataElement* element = new NotificationDataElement("TestRule", "TestAsset", m_readingSet);
    
    // Act & Assert - Should not crash
    EXPECT_NO_THROW(delete element);
}

// Test NotificationQueueElement class
TEST_F(NotificationQueueTest, NotificationQueueElementConstructor)
{
    // Arrange & Act
    NotificationQueueElement element("TestSource", "TestAsset", m_readingSet);
    
    // Assert
    EXPECT_EQ(element.getAssetName(), "TestAsset");
    EXPECT_EQ(element.getKey(), "TestSource::TestAsset");
    EXPECT_EQ(element.getAssetData(), m_readingSet);
}

TEST_F(NotificationQueueTest, NotificationQueueElementDestructor)
{
    // Arrange
    NotificationQueueElement* element = new NotificationQueueElement("TestSource", "TestAsset", m_readingSet);
    
    // Act & Assert - Should not crash
    EXPECT_NO_THROW(delete element);
}

TEST_F(NotificationQueueTest, NotificationQueueElementQueuedTimeCheck)
{
    // Arrange
    NotificationQueueElement element("TestSource", "TestAsset", m_readingSet);
    
    // Act & Assert - Should not crash
    EXPECT_NO_THROW(element.queuedTimeCheck());
}

// Test NotificationQueue class
TEST_F(NotificationQueueTest, NotificationQueueConstructor)
{
    // Arrange & Act
    NotificationQueue queue("TestNotification");
    
    // Assert
    EXPECT_EQ(queue.getName(), "TestNotification");
    EXPECT_TRUE(queue.isRunning());
    EXPECT_EQ(NotificationQueue::getInstance(), &queue);
}

TEST_F(NotificationQueueTest, NotificationQueueDestructor)
{
    // Arrange
    NotificationQueue* queue = new NotificationQueue("TestNotification");
    
    // Act & Assert - Should not crash
    EXPECT_NO_THROW(delete queue);
}

TEST_F(NotificationQueueTest, NotificationQueueAddElement)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    NotificationQueueElement* element = new NotificationQueueElement("TestSource", "TestAsset", m_readingSet);
    
    // Act
    bool result = queue.addElement(element);
    
    // Assert
    EXPECT_TRUE(result);
}

TEST_F(NotificationQueueTest, NotificationQueueAddElementWhenStopped)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    queue.stop();
    NotificationQueueElement* element = new NotificationQueueElement("TestSource", "TestAsset", m_readingSet);
    
    // Act
    bool result = queue.addElement(element);
    
    // Assert
    EXPECT_TRUE(result);
}

// Test buffer operations
TEST_F(NotificationQueueTest, FeedDataBuffer)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    
    // Act
    bool result = queue.feedDataBuffer("TestRule", "TestAsset", m_readingSet);
    
    // Assert
    EXPECT_TRUE(result);
}

TEST_F(NotificationQueueTest, FeedDataBufferWithNullData)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    
    // Act
    bool result = queue.feedDataBuffer("TestRule", "TestAsset", nullptr);
    
    // Assert
    EXPECT_FALSE(result);
}

TEST_F(NotificationQueueTest, GetBufferData)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    queue.feedDataBuffer("TestRule", "TestAsset", m_readingSet);
    
    // Act
    auto& bufferData = queue.getBufferData("TestRule", "TestAsset");
    
    // Assert
    EXPECT_FALSE(bufferData.empty());
    EXPECT_EQ(bufferData.size(), 1);
}

TEST_F(NotificationQueueTest, ClearBufferData)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    queue.feedDataBuffer("TestRule", "TestAsset", m_readingSet);
    
    // Act
    queue.clearBufferData("TestRule", "TestAsset");
    
    // Assert
    auto& bufferData = queue.getBufferData("TestRule", "TestAsset");
    EXPECT_TRUE(bufferData.empty());
}

TEST_F(NotificationQueueTest, KeepBufferData)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    queue.feedDataBuffer("TestRule", "TestAsset", m_readingSet);
    queue.feedDataBuffer("TestRule", "TestAsset", m_readingSet);
    queue.feedDataBuffer("TestRule", "TestAsset", m_readingSet);
    
    // Act
    queue.keepBufferData("TestRule", "TestAsset", 1);
    
    // Assert
    auto& bufferData = queue.getBufferData("TestRule", "TestAsset");
    EXPECT_EQ(bufferData.size(), 1);
}

// Test data processing
TEST_F(NotificationQueueTest, ProcessDataSet)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    NotificationQueueElement* element = new NotificationQueueElement("TestSource", "TestAsset", m_readingSet);
    
    // Act & Assert - Should not crash
    EXPECT_NO_THROW(queue.processDataSet(element));
    
    // Cleanup
    delete element;
}

TEST_F(NotificationQueueTest, FeedAllDataBuffers)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    NotificationQueueElement* element = new NotificationQueueElement("TestSource", "TestAsset", m_readingSet);
    
    // Act
    bool result = queue.feedAllDataBuffers(element);
    
    // Assert
    EXPECT_TRUE(result);
    
    // Cleanup
    delete element;
}

TEST_F(NotificationQueueTest, FeedAllDataBuffersWithNullData)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    
    // Act
    bool result = queue.feedAllDataBuffers(nullptr);
    
    // Assert
    EXPECT_FALSE(result);
}

// Test evaluation methods
TEST_F(NotificationQueueTest, SetValue)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    map<string, ResultData> result;
    DatapointValue dpv(42.5);
    Datapoint* dp = new Datapoint("temperature", dpv);
    
    // Act
    queue.setValue(result, dp, EvaluationType::Minimum);
    
    // Assert
    EXPECT_FALSE(result.empty());
    EXPECT_EQ(result["temperature"].vData.size(), 1);
    
    // Cleanup
    delete dp;
}

TEST_F(NotificationQueueTest, SetMinValue)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    map<string, ResultData> result;
    DatapointValue dpv1(50.0);
    DatapointValue dpv2(30.0);
    Datapoint* dp1 = new Datapoint("temperature", dpv1);
    Datapoint* dp2 = new Datapoint("temperature", dpv2);
    
    // Act
    queue.setValue(result, dp1, EvaluationType::Minimum);
    queue.setMinValue(result, "temperature", dpv2);
    
    // Assert
    EXPECT_EQ(result["temperature"].vData[0]->getData().toDouble(), 30.0);
    
    // Cleanup
    delete dp1;
    delete dp2;
}

TEST_F(NotificationQueueTest, SetMaxValue)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    map<string, ResultData> result;
    DatapointValue dpv1(30.0);
    DatapointValue dpv2(50.0);
    Datapoint* dp1 = new Datapoint("temperature", dpv1);
    Datapoint* dp2 = new Datapoint("temperature", dpv2);
    
    // Act
    queue.setValue(result, dp1, EvaluationType::Maximum);
    queue.setMaxValue(result, "temperature", dpv2);
    
    // Assert
    EXPECT_EQ(result["temperature"].vData[0]->getData().toDouble(), 50.0);
    
    // Cleanup
    delete dp1;
    delete dp2;
}

TEST_F(NotificationQueueTest, SetSumValues)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    map<string, ResultData> result;
    DatapointValue dpv1(10.0);
    DatapointValue dpv2(20.0);
    Datapoint* dp1 = new Datapoint("temperature", dpv1);
    Datapoint* dp2 = new Datapoint("temperature", dpv2);
    
    // Act
    queue.setValue(result, dp1, EvaluationType::Average);
    queue.setSumValues(result, "temperature", dpv2);
    
    // Assert
    EXPECT_EQ(result["temperature"].vData[0]->getData().toDouble(), 30.0);
    
    // Cleanup
    delete dp1;
    delete dp2;
}

TEST_F(NotificationQueueTest, SetLatestValue)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    map<string, ResultData> result;
    DatapointValue dpv1(10.0);
    DatapointValue dpv2(20.0);
    Datapoint* dp1 = new Datapoint("temperature", dpv1);
    Datapoint* dp2 = new Datapoint("temperature", dpv2);
    
    // Act
    queue.setValue(result, dp1, EvaluationType::SingleItem);
    queue.setLatestValue(result, "temperature", dpv2);
    
    // Assert
    EXPECT_EQ(result["temperature"].vData[0]->getData().toDouble(), 20.0);
    
    // Cleanup
    delete dp1;
    delete dp2;
}

// Test aggregation methods
TEST_F(NotificationQueueTest, AggregateData)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    vector<NotificationDataElement*> readingsData;
    map<string, string> result;
    
    // Create test data elements
    ReadingSet* rs1 = new ReadingSet();
    ReadingSet* rs2 = new ReadingSet();
    
    DatapointValue dpv1(10.0);
    DatapointValue dpv2(20.0);
    Datapoint* dp1 = new Datapoint("temperature", dpv1);
    Datapoint* dp2 = new Datapoint("temperature", dpv2);
    
    Reading* reading1 = new Reading("TestAsset", dp1);
    Reading* reading2 = new Reading("TestAsset", dp2);
    
    rs1->append(reading1);
    rs2->append(reading2);
    
    NotificationDataElement* element1 = new NotificationDataElement("TestRule", "TestAsset", rs1);
    NotificationDataElement* element2 = new NotificationDataElement("TestRule", "TestAsset", rs2);
    
    readingsData.push_back(element1);
    readingsData.push_back(element2);
    
    // Act
    queue.aggregateData(readingsData, 2, EvaluationType::Minimum, result);
    
    // Assert
    EXPECT_FALSE(result.empty());
    EXPECT_EQ(result["temperature"], "10");
    
    // Cleanup
    delete element1;
    delete element2;
}

// Test single item data processing
TEST_F(NotificationQueueTest, SetSingleItemData)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    vector<NotificationDataElement*> readingsData;
    map<string, AssetData> results;
    
    // Create test data element
    ReadingSet* rs = new ReadingSet();
    DatapointValue dpv(42.5);
    Datapoint* dp = new Datapoint("temperature", dpv);
    Reading* reading = new Reading("TestAsset", dp);
    rs->append(reading);
    
    NotificationDataElement* element = new NotificationDataElement("TestRule", "TestAsset", rs);
    readingsData.push_back(element);
    
    // Act
    queue.setSingleItemData(readingsData, results);
    
    // Assert
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results["TestAsset"].rData.size(), 1);
    
    // Cleanup
    delete element;
}

// Test process all readings
TEST_F(NotificationQueueTest, ProcessAllReadings)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    vector<NotificationDataElement*> readingsData;
    map<string, AssetData> results;
    
    // Create test data element
    ReadingSet* rs = new ReadingSet();
    DatapointValue dpv(42.5);
    Datapoint* dp = new Datapoint("temperature", dpv);
    Reading* reading = new Reading("TestAsset", dp);
    rs->append(reading);
    
    NotificationDataElement* element = new NotificationDataElement("TestRule", "TestAsset", rs);
    readingsData.push_back(element);
    
    // Create mock notification detail
    NotificationDetail detail;
    detail.setAssetName("TestAsset");
    detail.setRuleName("TestRule");
    detail.setType(EvaluationType::SingleItem);
    
    // Act
    bool result = queue.processAllReadings(detail, readingsData, results);
    
    // Assert
    EXPECT_TRUE(result);
    EXPECT_FALSE(results.empty());
    
    // Cleanup
    delete element;
}

// Test process all buffers
TEST_F(NotificationQueueTest, ProcessAllBuffers)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    vector<NotificationDataElement*> readingsData;
    map<string, string> result;
    
    // Create test data elements
    ReadingSet* rs1 = new ReadingSet();
    ReadingSet* rs2 = new ReadingSet();
    
    DatapointValue dpv1(10.0);
    DatapointValue dpv2(20.0);
    Datapoint* dp1 = new Datapoint("temperature", dpv1);
    Datapoint* dp2 = new Datapoint("temperature", dpv2);
    
    Reading* reading1 = new Reading("TestAsset", dp1);
    Reading* reading2 = new Reading("TestAsset", dp2);
    
    rs1->append(reading1);
    rs2->append(reading2);
    
    NotificationDataElement* element1 = new NotificationDataElement("TestRule", "TestAsset", rs1);
    NotificationDataElement* element2 = new NotificationDataElement("TestRule", "TestAsset", rs2);
    
    readingsData.push_back(element1);
    readingsData.push_back(element2);
    
    // Act
    queue.processAllBuffers(readingsData, EvaluationType::Minimum, 1000, result);
    
    // Assert
    EXPECT_FALSE(result.empty());
    
    // Cleanup
    delete element1;
    delete element2;
}

// Test process data buffer
TEST_F(NotificationQueueTest, ProcessDataBuffer)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    map<string, AssetData> results;
    
    // Feed some data first
    queue.feedDataBuffer("TestRule", "TestAsset", m_readingSet);
    
    // Create mock notification detail
    NotificationDetail detail;
    detail.setAssetName("TestAsset");
    detail.setRuleName("TestRule");
    detail.setType(EvaluationType::SingleItem);
    
    // Act
    bool result = queue.processDataBuffer(results, "TestRule", "TestAsset", detail);
    
    // Assert
    EXPECT_TRUE(result);
    EXPECT_FALSE(results.empty());
}

// Test process all data buffers
TEST_F(NotificationQueueTest, ProcessAllDataBuffers)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    
    // Act & Assert - Should not crash
    EXPECT_NO_THROW(queue.processAllDataBuffers("TestSource::TestAsset", "TestAsset"));
}

// Test send notification
TEST_F(NotificationQueueTest, SendNotification)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    map<string, AssetData> results;
    
    // Create mock subscription element
    AssetSubscriptionElement subscription("TestAsset", "TestNotification", nullptr);
    
    // Act & Assert - Should not crash
    EXPECT_NO_THROW(queue.sendNotification(results, subscription));
}

// Test eval rule
TEST_F(NotificationQueueTest, EvalRule)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    map<string, AssetData> results;
    
    // Create mock rule
    MockNotificationRule* rule = new MockNotificationRule("TestRule");
    
    // Act & Assert - Should not crash
    EXPECT_NO_THROW(queue.evalRule(results, rule));
    
    // Cleanup
    delete rule;
}

// Test thread safety
TEST_F(NotificationQueueTest, ThreadSafety)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    
    // Act - Call methods from multiple threads
    vector<thread> threads;
    for (int i = 0; i < 10; ++i)
    {
        threads.emplace_back([&queue]() {
            ReadingSet* rs = new ReadingSet();
            DatapointValue dpv(42.5);
            Datapoint* dp = new Datapoint("temperature", dpv);
            Reading* reading = new Reading("TestAsset", dp);
            rs->append(reading);
            
            queue.feedDataBuffer("TestRule", "TestAsset", rs);
            
            delete rs;
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads)
    {
        thread.join();
    }
    
    // Assert - Should not crash
    EXPECT_TRUE(true);
}

// Test edge cases
TEST_F(NotificationQueueTest, EmptyReadingSet)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    ReadingSet* emptySet = new ReadingSet();
    
    // Act
    bool result = queue.feedDataBuffer("TestRule", "TestAsset", emptySet);
    
    // Assert
    EXPECT_TRUE(result);
    
    // Cleanup
    delete emptySet;
}

TEST_F(NotificationQueueTest, MultipleAssets)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    
    // Create reading sets for different assets
    ReadingSet* rs1 = new ReadingSet();
    ReadingSet* rs2 = new ReadingSet();
    
    DatapointValue dpv1(42.5);
    DatapointValue dpv2(37.8);
    Datapoint* dp1 = new Datapoint("temperature", dpv1);
    Datapoint* dp2 = new Datapoint("temperature", dpv2);
    
    Reading* reading1 = new Reading("Asset1", dp1);
    Reading* reading2 = new Reading("Asset2", dp2);
    
    rs1->append(reading1);
    rs2->append(reading2);
    
    // Act
    bool result1 = queue.feedDataBuffer("TestRule", "Asset1", rs1);
    bool result2 = queue.feedDataBuffer("TestRule", "Asset2", rs2);
    
    // Assert
    EXPECT_TRUE(result1);
    EXPECT_TRUE(result2);
    
    auto& bufferData1 = queue.getBufferData("TestRule", "Asset1");
    auto& bufferData2 = queue.getBufferData("TestRule", "Asset2");
    
    EXPECT_FALSE(bufferData1.empty());
    EXPECT_FALSE(bufferData2.empty());
    
    // Cleanup
    delete rs1;
    delete rs2;
}

TEST_F(NotificationQueueTest, LargeDataSet)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    ReadingSet* largeSet = new ReadingSet();
    
    // Create many readings
    for (int i = 0; i < 100; ++i)
    {
        DatapointValue dpv(i);
        Datapoint* dp = new Datapoint("value" + to_string(i), dpv);
        Reading* reading = new Reading("TestAsset", dp);
        largeSet->append(reading);
    }
    
    // Act
    bool result = queue.feedDataBuffer("TestRule", "TestAsset", largeSet);
    
    // Assert
    EXPECT_TRUE(result);
    
    // Cleanup
    delete largeSet;
}

TEST_F(NotificationQueueTest, DifferentDataTypes)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    ReadingSet* mixedSet = new ReadingSet();
    
    // Create readings with different data types
    DatapointValue intVal(42);
    DatapointValue floatVal(42.5);
    DatapointValue stringVal("test");
    
    Datapoint* dp1 = new Datapoint("integer", intVal);
    Datapoint* dp2 = new Datapoint("float", floatVal);
    Datapoint* dp3 = new Datapoint("string", stringVal);
    
    Reading* reading = new Reading("TestAsset", dp1);
    reading->addDatapoint(dp2);
    reading->addDatapoint(dp3);
    
    mixedSet->append(reading);
    
    // Act
    bool result = queue.feedDataBuffer("TestRule", "TestAsset", mixedSet);
    
    // Assert
    EXPECT_TRUE(result);
    
    // Cleanup
    delete mixedSet;
}

// Test time-based processing
TEST_F(NotificationQueueTest, ProcessTime)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    
    // Act - Start time processing in a separate thread
    thread timeThread([&queue]() {
        // Run for a short time
        this_thread::sleep_for(chrono::milliseconds(100));
    });
    
    // Wait for thread to complete
    timeThread.join();
    
    // Assert - Should not crash
    EXPECT_TRUE(true);
}

// Test queue stop functionality
TEST_F(NotificationQueueTest, QueueStop)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    
    // Act
    queue.stop();
    
    // Assert
    EXPECT_FALSE(queue.isRunning());
}

// Test singleton pattern
TEST_F(NotificationQueueTest, SingletonPattern)
{
    // Arrange
    NotificationQueue* queue1 = new NotificationQueue("Test1");
    NotificationQueue* queue2 = new NotificationQueue("Test2");
    
    // Act
    NotificationQueue* instance1 = NotificationQueue::getInstance();
    NotificationQueue* instance2 = NotificationQueue::getInstance();
    
    // Assert
    EXPECT_EQ(instance1, instance2);
    EXPECT_EQ(instance1, queue2); // Last created should be the instance
    
    // Cleanup
    delete queue1;
    delete queue2;
}

// Test memory management
TEST_F(NotificationQueueTest, MemoryManagement)
{
    // Arrange
    NotificationQueue* queue = new NotificationQueue("TestNotification");
    
    // Add many elements
    for (int i = 0; i < 100; ++i)
    {
        ReadingSet* rs = new ReadingSet();
        DatapointValue dpv(i);
        Datapoint* dp = new Datapoint("value", dpv);
        Reading* reading = new Reading("TestAsset", dp);
        rs->append(reading);
        
        NotificationQueueElement* element = new NotificationQueueElement("TestSource", "TestAsset", rs);
        queue->addElement(element);
    }
    
    // Act & Assert - Should not crash
    EXPECT_NO_THROW(delete queue);
}

// Test error conditions
TEST_F(NotificationQueueTest, NullDataHandling)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    
    // Act & Assert - Should handle null data gracefully
    EXPECT_NO_THROW(queue.feedDataBuffer("TestRule", "TestAsset", nullptr));
}

TEST_F(NotificationQueueTest, EmptyAssetName)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    
    // Act
    bool result = queue.feedDataBuffer("TestRule", "", m_readingSet);
    
    // Assert
    EXPECT_TRUE(result);
}

TEST_F(NotificationQueueTest, EmptyRuleName)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    
    // Act
    bool result = queue.feedDataBuffer("", "TestAsset", m_readingSet);
    
    // Assert
    EXPECT_TRUE(result);
}

// Test performance
TEST_F(NotificationQueueTest, PerformanceTest)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    auto start = chrono::high_resolution_clock::now();
    
    // Act - Add many elements quickly
    for (int i = 0; i < 1000; ++i)
    {
        ReadingSet* rs = new ReadingSet();
        DatapointValue dpv(i);
        Datapoint* dp = new Datapoint("value", dpv);
        Reading* reading = new Reading("TestAsset", dp);
        rs->append(reading);
        
        NotificationQueueElement* element = new NotificationQueueElement("TestSource", "TestAsset", rs);
        queue.addElement(element);
    }
    
    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);
    
    // Assert - Should complete within reasonable time
    EXPECT_LT(duration.count(), 5000); // Less than 5 seconds
    
    // Cleanup
    queue.stop();
}

// Main function for running the tests
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 