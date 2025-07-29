#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <thread>

#include "notification_subscription.h"
#include "notification_manager.h"
#include "notification_api.h"
#include "storage_client.h"
#include "logger.h"

using namespace std;

// Mock classes for testing
class MockStorageClient : public StorageClient
{
public:
    MockStorageClient() : StorageClient("localhost", 8080), m_registerAssetCalled(false), m_unregisterAssetCalled(false),
                         m_registerTableCalled(false), m_unregisterTableCalled(false) {}
    
    bool registerAssetNotification(const string& assetName, const string& callbackUrl)
    {
        m_registerAssetCalled = true;
        m_lastAsset = assetName;
        m_lastUrl = callbackUrl;
        return true;
    }
    
    bool unregisterAssetNotification(const string& assetName, const string& callbackUrl)
    {
        m_unregisterAssetCalled = true;
        m_lastAsset = assetName;
        m_lastUrl = callbackUrl;
        return true;
    }
    
    bool registerTableNotification(const string& tableName, const string& key, 
                                 vector<string> keyValues, const string& operation, 
                                 const string& callbackUrl)
    {
        m_registerTableCalled = true;
        m_lastTable = tableName;
        m_lastColumn = key;
        m_lastKeyValues = keyValues;
        m_lastOperation = operation;
        m_lastUrl = callbackUrl;
        return true;
    }
    
    bool unregisterTableNotification(const string& tableName, const string& key,
                                   vector<string> keyValues, const string& operation,
                                   const string& callbackUrl)
    {
        m_unregisterTableCalled = true;
        m_lastTable = tableName;
        m_lastColumn = key;
        m_lastKeyValues = keyValues;
        m_lastOperation = operation;
        m_lastUrl = callbackUrl;
        return true;
    }
    
    // Test helper methods
    bool wasRegisterAssetCalled() const { return m_registerAssetCalled; }
    bool wasUnregisterAssetCalled() const { return m_unregisterAssetCalled; }
    bool wasRegisterTableCalled() const { return m_registerTableCalled; }
    bool wasUnregisterTableCalled() const { return m_unregisterTableCalled; }
    
    string getLastAsset() const { return m_lastAsset; }
    string getLastUrl() const { return m_lastUrl; }
    string getLastTable() const { return m_lastTable; }
    string getLastColumn() const { return m_lastColumn; }
    vector<string> getLastKeyValues() const { return m_lastKeyValues; }
    string getLastOperation() const { return m_lastOperation; }
    
    void reset()
    {
        m_registerAssetCalled = false;
        m_unregisterAssetCalled = false;
        m_registerTableCalled = false;
        m_unregisterTableCalled = false;
        m_lastAsset.clear();
        m_lastUrl.clear();
        m_lastTable.clear();
        m_lastColumn.clear();
        m_lastKeyValues.clear();
        m_lastOperation.clear();
    }

private:
    bool m_registerAssetCalled;
    bool m_unregisterAssetCalled;
    bool m_registerTableCalled;
    bool m_unregisterTableCalled;
    string m_lastAsset;
    string m_lastUrl;
    string m_lastTable;
    string m_lastColumn;
    vector<string> m_lastKeyValues;
    string m_lastOperation;
};

class MockNotificationInstance : public NotificationInstance
{
public:
    MockNotificationInstance(const string& name) : 
        NotificationInstance(name, true, NotificationInstance::NotificationType{NotificationInstance::OneShot, {0, 0}}, nullptr, nullptr),
        m_name(name) {}
    
    string getName() const { return m_name; }
    NotificationRule* getRule() { return nullptr; }
    NotificationDelivery* getDelivery() { return nullptr; }

private:
    string m_name;
};

// Test fixture for notification subscription tests
class NotificationSubscriptionTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Initialize logger for tests
        Logger::getLogger();
        
        // Create mock storage client
        m_storageClient = unique_ptr<MockStorageClient>(new MockStorageClient());
        
        // Create test notification instance
        m_notificationInstance = unique_ptr<MockNotificationInstance>(new MockNotificationInstance("TestNotification"));
    }
    
    void TearDown() override
    {
        // Cleanup if needed
    }
    
    unique_ptr<MockStorageClient> m_storageClient;
    unique_ptr<MockNotificationInstance> m_notificationInstance;
};

// Test SubscriptionElement base class - using concrete implementation
TEST_F(NotificationSubscriptionTest, SubscriptionElementConstructor)
{
    // Arrange & Act
    AssetSubscriptionElement element("TestAsset", "TestNotification", nullptr);
    
    // Assert
    EXPECT_EQ(element.getNotificationName(), "TestNotification");
    EXPECT_EQ(element.getInstance(), nullptr);
    EXPECT_EQ(element.getRule(), nullptr);
    EXPECT_EQ(element.getDelivery(), nullptr);
}

TEST_F(NotificationSubscriptionTest, SubscriptionElementWithInstance)
{
    // Arrange
    MockNotificationInstance* instance = m_notificationInstance.get();
    
    // Act
    AssetSubscriptionElement element("TestAsset", "TestNotification", instance);
    
    // Assert
    EXPECT_EQ(element.getNotificationName(), "TestNotification");
    EXPECT_EQ(element.getInstance(), (NotificationInstance*)instance);
}

// Test AssetSubscriptionElement
TEST_F(NotificationSubscriptionTest, AssetSubscriptionElementConstructor)
{
    // Arrange & Act
    AssetSubscriptionElement element("TestAsset", "TestNotification", nullptr);
    
    // Assert
    EXPECT_EQ(element.getNotificationName(), "TestNotification");
    EXPECT_EQ(element.getAssetName(), "TestAsset");
    EXPECT_EQ(element.getKey(), "asset::TestAsset");
}

TEST_F(NotificationSubscriptionTest, AssetSubscriptionElementRegister)
{
    // Arrange
    AssetSubscriptionElement element("TestAsset", "TestNotification", nullptr);
    
    // Act
    bool result = element.registerSubscription(*m_storageClient);
    
    // Assert
    EXPECT_TRUE(result);
    EXPECT_TRUE(m_storageClient->wasRegisterAssetCalled());
    EXPECT_EQ(m_storageClient->getLastAsset(), "TestAsset");
    EXPECT_FALSE(m_storageClient->getLastUrl().empty());
}

TEST_F(NotificationSubscriptionTest, AssetSubscriptionElementUnregister)
{
    // Arrange
    AssetSubscriptionElement element("TestAsset", "TestNotification", nullptr);
    
    // Act
    bool result = element.unregister(*m_storageClient);
    
    // Assert
    EXPECT_TRUE(result);
    EXPECT_TRUE(m_storageClient->wasUnregisterAssetCalled());
    EXPECT_EQ(m_storageClient->getLastAsset(), "TestAsset");
    EXPECT_FALSE(m_storageClient->getLastUrl().empty());
}

TEST_F(NotificationSubscriptionTest, AssetSubscriptionElementGetKey)
{
    // Arrange
    AssetSubscriptionElement element("TestAsset", "TestNotification", nullptr);
    
    // Act
    string key = element.getKey();
    
    // Assert
    EXPECT_EQ(key, "asset::TestAsset");
}

// Test AuditSubscriptionElement
TEST_F(NotificationSubscriptionTest, AuditSubscriptionElementConstructor)
{
    // Arrange & Act
    AuditSubscriptionElement element("AUDIT001", "TestNotification", nullptr);
    
    // Assert
    EXPECT_EQ(element.getNotificationName(), "TestNotification");
    EXPECT_EQ(element.getAuditCode(), "AUDIT001");
    EXPECT_EQ(element.getKey(), "audit::AUDIT001");
}

TEST_F(NotificationSubscriptionTest, AuditSubscriptionElementRegister)
{
    // Arrange
    AuditSubscriptionElement element("AUDIT001", "TestNotification", nullptr);
    
    // Act
    bool result = element.registerSubscription(*m_storageClient);
    
    // Assert
    EXPECT_TRUE(result);
    EXPECT_TRUE(m_storageClient->wasRegisterTableCalled());
    EXPECT_EQ(m_storageClient->getLastTable(), "log");
    EXPECT_EQ(m_storageClient->getLastColumn(), "code");
    EXPECT_EQ(m_storageClient->getLastOperation(), "insert");
    EXPECT_EQ(m_storageClient->getLastKeyValues().size(), 1);
    EXPECT_EQ(m_storageClient->getLastKeyValues()[0], "AUDIT001");
}

TEST_F(NotificationSubscriptionTest, AuditSubscriptionElementUnregister)
{
    // Arrange
    AuditSubscriptionElement element("AUDIT001", "TestNotification", nullptr);
    
    // Act
    bool result = element.unregister(*m_storageClient);
    
    // Assert
    EXPECT_TRUE(result);
    EXPECT_TRUE(m_storageClient->wasUnregisterTableCalled());
    EXPECT_EQ(m_storageClient->getLastTable(), "log");
    EXPECT_EQ(m_storageClient->getLastColumn(), "code");
    EXPECT_EQ(m_storageClient->getLastOperation(), "insert");
    EXPECT_EQ(m_storageClient->getLastKeyValues().size(), 1);
    EXPECT_EQ(m_storageClient->getLastKeyValues()[0], "AUDIT001");
}

TEST_F(NotificationSubscriptionTest, AuditSubscriptionElementGetKey)
{
    // Arrange
    AuditSubscriptionElement element("AUDIT001", "TestNotification", nullptr);
    
    // Act
    string key = element.getKey();
    
    // Assert
    EXPECT_EQ(key, "audit::AUDIT001");
}

// Test StatsSubscriptionElement
TEST_F(NotificationSubscriptionTest, StatsSubscriptionElementConstructor)
{
    // Arrange & Act
    StatsSubscriptionElement element("READINGS", "TestNotification", nullptr);
    
    // Assert
    EXPECT_EQ(element.getNotificationName(), "TestNotification");
    EXPECT_EQ(element.getStatistic(), "READINGS");
    EXPECT_EQ(element.getKey(), "stat::READINGS");
}

TEST_F(NotificationSubscriptionTest, StatsSubscriptionElementRegister)
{
    // Arrange
    StatsSubscriptionElement element("READINGS", "TestNotification", nullptr);
    
    // Act
    bool result = element.registerSubscription(*m_storageClient);
    
    // Assert
    EXPECT_TRUE(result);
    EXPECT_TRUE(m_storageClient->wasRegisterTableCalled());
    EXPECT_EQ(m_storageClient->getLastTable(), "statistics");
    EXPECT_EQ(m_storageClient->getLastColumn(), "key");
    EXPECT_EQ(m_storageClient->getLastOperation(), "update");
    EXPECT_EQ(m_storageClient->getLastKeyValues().size(), 1);
    EXPECT_EQ(m_storageClient->getLastKeyValues()[0], "READINGS");
}

TEST_F(NotificationSubscriptionTest, StatsSubscriptionElementUnregister)
{
    // Arrange
    StatsSubscriptionElement element("READINGS", "TestNotification", nullptr);
    
    // Act
    bool result = element.unregister(*m_storageClient);
    
    // Assert
    EXPECT_TRUE(result);
    EXPECT_TRUE(m_storageClient->wasUnregisterTableCalled());
    EXPECT_EQ(m_storageClient->getLastTable(), "statistics");
    EXPECT_EQ(m_storageClient->getLastColumn(), "key");
    EXPECT_EQ(m_storageClient->getLastOperation(), "update");
    EXPECT_EQ(m_storageClient->getLastKeyValues().size(), 1);
    EXPECT_EQ(m_storageClient->getLastKeyValues()[0], "READINGS");
}

TEST_F(NotificationSubscriptionTest, StatsSubscriptionElementGetKey)
{
    // Arrange
    StatsSubscriptionElement element("READINGS", "TestNotification", nullptr);
    
    // Act
    string key = element.getKey();
    
    // Assert
    EXPECT_EQ(key, "stat::READINGS");
}

// Test StatsRateSubscriptionElement
TEST_F(NotificationSubscriptionTest, StatsRateSubscriptionElementConstructor)
{
    // Arrange & Act
    StatsRateSubscriptionElement element("READINGS", "TestNotification", nullptr);
    
    // Assert
    EXPECT_EQ(element.getNotificationName(), "TestNotification");
    EXPECT_EQ(element.getStatistic(), "READINGS");
    EXPECT_EQ(element.getKey(), "rate::READINGS");
}

TEST_F(NotificationSubscriptionTest, StatsRateSubscriptionElementRegister)
{
    // Arrange
    StatsRateSubscriptionElement element("READINGS", "TestNotification", nullptr);
    
    // Act
    bool result = element.registerSubscription(*m_storageClient);
    
    // Assert
    EXPECT_TRUE(result);
    EXPECT_TRUE(m_storageClient->wasRegisterTableCalled());
    EXPECT_EQ(m_storageClient->getLastTable(), "statistics");
    EXPECT_EQ(m_storageClient->getLastColumn(), "key");
    EXPECT_EQ(m_storageClient->getLastOperation(), "update");
    EXPECT_EQ(m_storageClient->getLastKeyValues().size(), 1);
    EXPECT_EQ(m_storageClient->getLastKeyValues()[0], "READINGS");
}

TEST_F(NotificationSubscriptionTest, StatsRateSubscriptionElementUnregister)
{
    // Arrange
    StatsRateSubscriptionElement element("READINGS", "TestNotification", nullptr);
    
    // Act
    bool result = element.unregister(*m_storageClient);
    
    // Assert
    EXPECT_TRUE(result);
    EXPECT_TRUE(m_storageClient->wasUnregisterTableCalled());
    EXPECT_EQ(m_storageClient->getLastTable(), "statistics");
    EXPECT_EQ(m_storageClient->getLastColumn(), "key");
    EXPECT_EQ(m_storageClient->getLastOperation(), "update");
    EXPECT_EQ(m_storageClient->getLastKeyValues().size(), 1);
    EXPECT_EQ(m_storageClient->getLastKeyValues()[0], "READINGS");
}

TEST_F(NotificationSubscriptionTest, StatsRateSubscriptionElementGetKey)
{
    // Arrange
    StatsRateSubscriptionElement element("READINGS", "TestNotification", nullptr);
    
    // Act
    string key = element.getKey();
    
    // Assert
    EXPECT_EQ(key, "rate::READINGS");
}

// Test AlertSubscriptionElement
TEST_F(NotificationSubscriptionTest, AlertSubscriptionElementConstructor)
{
    // Arrange & Act
    AlertSubscriptionElement element("TestNotification", nullptr);
    
    // Assert
    EXPECT_EQ(element.getNotificationName(), "TestNotification");
    EXPECT_EQ(element.getKey(), "alert::alert");
}

TEST_F(NotificationSubscriptionTest, AlertSubscriptionElementRegister)
{
    // Arrange
    AlertSubscriptionElement element("TestNotification", nullptr);
    
    // Act
    bool result = element.registerSubscription(*m_storageClient);
    
    // Assert
    EXPECT_TRUE(result);
    EXPECT_TRUE(m_storageClient->wasRegisterTableCalled());
    EXPECT_EQ(m_storageClient->getLastTable(), "alerts");
    EXPECT_EQ(m_storageClient->getLastColumn(), "");
    EXPECT_EQ(m_storageClient->getLastOperation(), "update");
    EXPECT_EQ(m_storageClient->getLastKeyValues().size(), 0);
}

TEST_F(NotificationSubscriptionTest, AlertSubscriptionElementUnregister)
{
    // Arrange
    AlertSubscriptionElement element("TestNotification", nullptr);
    
    // Act
    bool result = element.unregister(*m_storageClient);
    
    // Assert
    EXPECT_TRUE(result);
    EXPECT_TRUE(m_storageClient->wasUnregisterTableCalled());
    EXPECT_EQ(m_storageClient->getLastTable(), "alerts");
    EXPECT_EQ(m_storageClient->getLastColumn(), "");
    EXPECT_EQ(m_storageClient->getLastOperation(), "insert");
    EXPECT_EQ(m_storageClient->getLastKeyValues().size(), 0);
}

TEST_F(NotificationSubscriptionTest, AlertSubscriptionElementGetKey)
{
    // Arrange
    AlertSubscriptionElement element("TestNotification", nullptr);
    
    // Act
    string key = element.getKey();
    
    // Assert
    EXPECT_EQ(key, "alert::alert");
}

// Test NotificationSubscription class
TEST_F(NotificationSubscriptionTest, NotificationSubscriptionConstructor)
{
    // Arrange & Act
    NotificationSubscription subscription("TestNotification", *m_storageClient);
    
    // Assert
    EXPECT_EQ(subscription.getNotificationName(), "TestNotification");
    EXPECT_EQ(NotificationSubscription::getInstance(), &subscription);
}

TEST_F(NotificationSubscriptionTest, NotificationSubscriptionAddAssetSubscription)
{
    // Arrange
    NotificationSubscription subscription("TestNotification", *m_storageClient);
    AssetSubscriptionElement* element = new AssetSubscriptionElement("TestAsset", "TestNotification", nullptr);
    
    // Act
    bool result = subscription.addSubscription(element);
    
    // Assert
    EXPECT_TRUE(result);
    
    // Cleanup
    delete element;
}

TEST_F(NotificationSubscriptionTest, NotificationSubscriptionAddAuditSubscription)
{
    // Arrange
    NotificationSubscription subscription("TestNotification", *m_storageClient);
    AuditSubscriptionElement* element = new AuditSubscriptionElement("AUDIT001", "TestNotification", nullptr);
    
    // Act
    bool result = subscription.addSubscription(element);
    
    // Assert
    EXPECT_TRUE(result);
    
    // Cleanup
    delete element;
}

TEST_F(NotificationSubscriptionTest, NotificationSubscriptionAddStatsSubscription)
{
    // Arrange
    NotificationSubscription subscription("TestNotification", *m_storageClient);
    StatsSubscriptionElement* element = new StatsSubscriptionElement("READINGS", "TestNotification", nullptr);
    
    // Act
    bool result = subscription.addSubscription(element);
    
    // Assert
    EXPECT_TRUE(result);
    
    // Cleanup
    delete element;
}

TEST_F(NotificationSubscriptionTest, NotificationSubscriptionAddAlertSubscription)
{
    // Arrange
    NotificationSubscription subscription("TestNotification", *m_storageClient);
    AlertSubscriptionElement* element = new AlertSubscriptionElement("TestNotification", nullptr);
    
    // Act
    bool result = subscription.addSubscription(element);
    
    // Assert
    EXPECT_TRUE(result);
    
    // Cleanup
    delete element;
}

TEST_F(NotificationSubscriptionTest, NotificationSubscriptionGetAllSubscriptions)
{
    // Arrange
    NotificationSubscription subscription("TestNotification", *m_storageClient);
    AssetSubscriptionElement* element = new AssetSubscriptionElement("TestAsset", "TestNotification", nullptr);
    subscription.addSubscription(element);
    
    // Act
    auto& subscriptions = subscription.getAllSubscriptions();
    
    // Assert
    EXPECT_FALSE(subscriptions.empty());
    EXPECT_EQ(subscriptions.size(), 1);
    
    // Cleanup
    delete element;
}

TEST_F(NotificationSubscriptionTest, NotificationSubscriptionGetSubscription)
{
    // Arrange
    NotificationSubscription subscription("TestNotification", *m_storageClient);
    AssetSubscriptionElement* element = new AssetSubscriptionElement("TestAsset", "TestNotification", nullptr);
    subscription.addSubscription(element);
    
    // Act
    auto& subscriptions = subscription.getAllSubscriptions();
    
    // Assert
    EXPECT_FALSE(subscriptions.empty());
    
    // Cleanup
    delete element;
}

TEST_F(NotificationSubscriptionTest, NotificationSubscriptionLockUnlock)
{
    // Arrange
    NotificationSubscription subscription("TestNotification", *m_storageClient);
    
    // Act & Assert - Should not throw
    EXPECT_NO_THROW(subscription.lockSubscriptions());
    EXPECT_NO_THROW(subscription.unlockSubscriptions());
}

TEST_F(NotificationSubscriptionTest, NotificationSubscriptionRemoveSubscription)
{
    // Arrange
    NotificationSubscription subscription("TestNotification", *m_storageClient);
    AssetSubscriptionElement* element = new AssetSubscriptionElement("TestAsset", "TestNotification", nullptr);
    subscription.addSubscription(element);
    
    // Act
    subscription.removeSubscription("asset", "TestAsset", "TestRule");
    
    // Assert
    auto& subscriptions = subscription.getAllSubscriptions();
    EXPECT_TRUE(subscriptions.empty());
    
    // Cleanup
    delete element;
}

// Test edge cases
TEST_F(NotificationSubscriptionTest, SubscriptionElementWithNullInstance)
{
    // Arrange & Act
    AssetSubscriptionElement element("TestAsset", "TestNotification", nullptr);
    
    // Assert
    EXPECT_EQ(element.getRule(), nullptr);
    EXPECT_EQ(element.getDelivery(), nullptr);
}

TEST_F(NotificationSubscriptionTest, MultipleSubscriptionsForSameAsset)
{
    // Arrange
    NotificationSubscription subscription("TestNotification", *m_storageClient);
    AssetSubscriptionElement* element1 = new AssetSubscriptionElement("TestAsset", "TestNotification", nullptr);
    AssetSubscriptionElement* element2 = new AssetSubscriptionElement("TestAsset", "TestNotification", nullptr);
    
    // Act
    bool result1 = subscription.addSubscription(element1);
    bool result2 = subscription.addSubscription(element2);
    
    // Assert
    EXPECT_TRUE(result1);
    EXPECT_TRUE(result2);
    
    auto& subscriptions = subscription.getAllSubscriptions();
    EXPECT_FALSE(subscriptions.empty());
    
    // Cleanup
    delete element1;
    delete element2;
}

TEST_F(NotificationSubscriptionTest, SubscriptionElementDestructor)
{
    // Arrange
    AssetSubscriptionElement* element = new AssetSubscriptionElement("TestAsset", "TestNotification", nullptr);
    
    // Act & Assert - Should not crash
    EXPECT_NO_THROW(delete element);
}

TEST_F(NotificationSubscriptionTest, NotificationSubscriptionDestructor)
{
    // Arrange
    NotificationSubscription* subscription = new NotificationSubscription("TestNotification", *m_storageClient);
    AssetSubscriptionElement* element = new AssetSubscriptionElement("TestAsset", "TestNotification", nullptr);
    subscription->addSubscription(element);
    
    // Act & Assert - Should not crash
    EXPECT_NO_THROW(delete subscription);
}

// Test URL encoding functionality
TEST_F(NotificationSubscriptionTest, UrlEncoding)
{
    // Arrange
    AssetSubscriptionElement element("Test Asset With Spaces", "TestNotification", nullptr);
    
    // Act
    bool result = element.registerSubscription(*m_storageClient);
    
    // Assert
    EXPECT_TRUE(result);
    EXPECT_TRUE(m_storageClient->wasRegisterAssetCalled());
    EXPECT_EQ(m_storageClient->getLastAsset(), "Test Asset With Spaces");
    // URL should be encoded
    EXPECT_NE(m_storageClient->getLastUrl().find("Test%20Asset%20With%20Spaces"), string::npos);
}

// Test special characters in asset names
TEST_F(NotificationSubscriptionTest, SpecialCharactersInAssetName)
{
    // Arrange
    AssetSubscriptionElement element("Test-Asset_With.Special@Chars", "TestNotification", nullptr);
    
    // Act
    bool result = element.registerSubscription(*m_storageClient);
    
    // Assert
    EXPECT_TRUE(result);
    EXPECT_TRUE(m_storageClient->wasRegisterAssetCalled());
    EXPECT_EQ(m_storageClient->getLastAsset(), "Test-Asset_With.Special@Chars");
}

// Test empty strings
TEST_F(NotificationSubscriptionTest, EmptyAssetName)
{
    // Arrange
    AssetSubscriptionElement element("", "TestNotification", nullptr);
    
    // Act
    bool result = element.registerSubscription(*m_storageClient);
    
    // Assert
    EXPECT_TRUE(result);
    EXPECT_TRUE(m_storageClient->wasRegisterAssetCalled());
    EXPECT_EQ(m_storageClient->getLastAsset(), "");
}

TEST_F(NotificationSubscriptionTest, EmptyAuditCode)
{
    // Arrange
    AuditSubscriptionElement element("", "TestNotification", nullptr);
    
    // Act
    bool result = element.registerSubscription(*m_storageClient);
    
    // Assert
    EXPECT_TRUE(result);
    EXPECT_TRUE(m_storageClient->wasRegisterTableCalled());
    EXPECT_EQ(m_storageClient->getLastKeyValues()[0], "");
}

// Test very long strings
TEST_F(NotificationSubscriptionTest, LongAssetName)
{
    // Arrange
    string longAssetName(1000, 'A');
    AssetSubscriptionElement element(longAssetName, "TestNotification", nullptr);
    
    // Act
    bool result = element.registerSubscription(*m_storageClient);
    
    // Assert
    EXPECT_TRUE(result);
    EXPECT_TRUE(m_storageClient->wasRegisterAssetCalled());
    EXPECT_EQ(m_storageClient->getLastAsset(), longAssetName);
}

// Test thread safety
TEST_F(NotificationSubscriptionTest, ThreadSafety)
{
    // Arrange
    NotificationSubscription subscription("TestNotification", *m_storageClient);
    
    // Act - Call lock/unlock from multiple threads
    vector<thread> threads;
    for (int i = 0; i < 10; ++i)
    {
        threads.emplace_back([&subscription]() {
            subscription.lockSubscriptions();
            subscription.unlockSubscriptions();
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

// Main function is provided by main.cpp 