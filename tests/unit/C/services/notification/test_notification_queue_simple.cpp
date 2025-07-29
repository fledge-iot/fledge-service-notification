#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <memory>
#include <map>

#include "notification_queue.h"
#include "logger.h"

using namespace std;

// Test fixture for notification queue tests
class NotificationQueueSimpleTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Initialize logger for tests
        Logger::getLogger();
    }
    
    void TearDown() override
    {
        // Cleanup if needed
    }
};

// Test NotificationDataElement class
TEST_F(NotificationQueueSimpleTest, NotificationDataElementConstructor)
{
    // Arrange & Act
    ReadingSet* rs = new ReadingSet();
    NotificationDataElement element("TestRule", "TestAsset", rs);
    
    // Assert
    EXPECT_EQ(element.getAssetName(), "TestAsset");
    EXPECT_EQ(element.getRuleName(), "TestRule");
    EXPECT_EQ(element.getData(), rs);
    EXPECT_GT(element.getTime(), 0);
    
    // Cleanup
    delete rs;
}

TEST_F(NotificationQueueSimpleTest, NotificationDataElementDestructor)
{
    // Arrange
    ReadingSet* rs = new ReadingSet();
    NotificationDataElement* element = new NotificationDataElement("TestRule", "TestAsset", rs);
    
    // Act & Assert - Should not crash
    EXPECT_NO_THROW(delete element);
}

// Test NotificationQueueElement class
TEST_F(NotificationQueueSimpleTest, NotificationQueueElementConstructor)
{
    // Arrange & Act
    ReadingSet* rs = new ReadingSet();
    NotificationQueueElement element("TestSource", "TestAsset", rs);
    
    // Assert
    EXPECT_EQ(element.getAssetName(), "TestAsset");
    EXPECT_EQ(element.getKey(), "TestSource::TestAsset");
    EXPECT_EQ(element.getAssetData(), rs);
    
    // Cleanup
    delete rs;
}

TEST_F(NotificationQueueSimpleTest, NotificationQueueElementDestructor)
{
    // Arrange
    ReadingSet* rs = new ReadingSet();
    NotificationQueueElement* element = new NotificationQueueElement("TestSource", "TestAsset", rs);
    
    // Act & Assert - Should not crash
    EXPECT_NO_THROW(delete element);
}

TEST_F(NotificationQueueSimpleTest, NotificationQueueElementQueuedTimeCheck)
{
    // Arrange
    ReadingSet* rs = new ReadingSet();
    NotificationQueueElement element("TestSource", "TestAsset", rs);
    
    // Act & Assert - Should not crash
    EXPECT_NO_THROW(element.queuedTimeCheck());
    
    // Cleanup
    delete rs;
}

// Test NotificationQueue class
TEST_F(NotificationQueueSimpleTest, NotificationQueueConstructor)
{
    // Arrange & Act
    NotificationQueue queue("TestNotification");
    
    // Assert
    EXPECT_EQ(queue.getName(), "TestNotification");
    EXPECT_TRUE(queue.isRunning());
    EXPECT_EQ(NotificationQueue::getInstance(), &queue);
}

TEST_F(NotificationQueueSimpleTest, NotificationQueueDestructor)
{
    // Arrange
    NotificationQueue* queue = new NotificationQueue("TestNotification");
    
    // Act & Assert - Should not crash
    EXPECT_NO_THROW(delete queue);
}

TEST_F(NotificationQueueSimpleTest, NotificationQueueAddElement)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    ReadingSet* rs = new ReadingSet();
    NotificationQueueElement* element = new NotificationQueueElement("TestSource", "TestAsset", rs);
    
    // Act
    bool result = queue.addElement(element);
    
    // Assert
    EXPECT_TRUE(result);
}

TEST_F(NotificationQueueSimpleTest, NotificationQueueAddElementWhenStopped)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    queue.stop();
    ReadingSet* rs = new ReadingSet();
    NotificationQueueElement* element = new NotificationQueueElement("TestSource", "TestAsset", rs);
    
    // Act
    bool result = queue.addElement(element);
    
    // Assert
    EXPECT_TRUE(result);
}

// Test buffer operations
TEST_F(NotificationQueueSimpleTest, FeedDataBuffer)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    ReadingSet* rs = new ReadingSet();
    
    // Act
    bool result = queue.feedDataBuffer("TestRule", "TestAsset", rs);
    
    // Assert
    EXPECT_TRUE(result);
    
    // Cleanup
    delete rs;
}

TEST_F(NotificationQueueSimpleTest, FeedDataBufferWithNullData)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    
    // Act
    bool result = queue.feedDataBuffer("TestRule", "TestAsset", nullptr);
    
    // Assert
    EXPECT_FALSE(result);
}

TEST_F(NotificationQueueSimpleTest, GetBufferData)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    ReadingSet* rs = new ReadingSet();
    queue.feedDataBuffer("TestRule", "TestAsset", rs);
    
    // Act
    auto& bufferData = queue.getBufferData("TestRule", "TestAsset");
    
    // Assert
    EXPECT_FALSE(bufferData.empty());
    EXPECT_EQ(bufferData.size(), 1);
    
    // Cleanup
    delete rs;
}

TEST_F(NotificationQueueSimpleTest, ClearBufferData)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    ReadingSet* rs = new ReadingSet();
    queue.feedDataBuffer("TestRule", "TestAsset", rs);
    
    // Act
    queue.clearBufferData("TestRule", "TestAsset");
    
    // Assert
    auto& bufferData = queue.getBufferData("TestRule", "TestAsset");
    EXPECT_TRUE(bufferData.empty());
    
    // Cleanup
    delete rs;
}

TEST_F(NotificationQueueSimpleTest, KeepBufferData)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    ReadingSet* rs1 = new ReadingSet();
    ReadingSet* rs2 = new ReadingSet();
    ReadingSet* rs3 = new ReadingSet();
    
    queue.feedDataBuffer("TestRule", "TestAsset", rs1);
    queue.feedDataBuffer("TestRule", "TestAsset", rs2);
    queue.feedDataBuffer("TestRule", "TestAsset", rs3);
    
    // Act
    queue.keepBufferData("TestRule", "TestAsset", 1);
    
    // Assert
    auto& bufferData = queue.getBufferData("TestRule", "TestAsset");
    EXPECT_EQ(bufferData.size(), 1);
    
    // Cleanup
    delete rs1;
    delete rs2;
    delete rs3;
}

// Test queue stop functionality
TEST_F(NotificationQueueSimpleTest, QueueStop)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    
    // Act
    queue.stop();
    
    // Assert
    EXPECT_FALSE(queue.isRunning());
}

// Test singleton pattern
TEST_F(NotificationQueueSimpleTest, SingletonPattern)
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

// Test error conditions
TEST_F(NotificationQueueSimpleTest, NullDataHandling)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    
    // Act & Assert - Should handle null data gracefully
    EXPECT_NO_THROW(queue.feedDataBuffer("TestRule", "TestAsset", nullptr));
}

TEST_F(NotificationQueueSimpleTest, EmptyAssetName)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    ReadingSet* rs = new ReadingSet();
    
    // Act
    bool result = queue.feedDataBuffer("TestRule", "", rs);
    
    // Assert
    EXPECT_TRUE(result);
    
    // Cleanup
    delete rs;
}

TEST_F(NotificationQueueSimpleTest, EmptyRuleName)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    ReadingSet* rs = new ReadingSet();
    
    // Act
    bool result = queue.feedDataBuffer("", "TestAsset", rs);
    
    // Assert
    EXPECT_TRUE(result);
    
    // Cleanup
    delete rs;
}

// Test edge cases
TEST_F(NotificationQueueSimpleTest, EmptyReadingSet)
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

TEST_F(NotificationQueueSimpleTest, MultipleAssets)
{
    // Arrange
    NotificationQueue queue("TestNotification");
    
    // Create reading sets for different assets
    ReadingSet* rs1 = new ReadingSet();
    ReadingSet* rs2 = new ReadingSet();
    
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

// Main function for running the tests
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 