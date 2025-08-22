#include <gtest/gtest.h>
#include "delivery_plugin.h"
#include "logger.h"

using namespace std;

class DeliveryPluginExpandMacrosTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_plugin = new DeliveryPlugin("test_delivery", NULL);
    }

    void TearDown() override
    {
        if (m_plugin)
        {
            delete m_plugin;
            m_plugin = nullptr;
        }
    }

    DeliveryPlugin* m_plugin;
};

// Test basic macro substitution with string values
TEST_F(DeliveryPluginExpandMacrosTest, BasicStringMacroSubstitution)
{
    string message = "Temperature is $temperature$ degrees";
    string reason = "{\"reason\": \"triggered\", \"auditCode\": \"test\", \"data\": { \"example\" : {\"temperature\": \"25.5\"}}}";
    
    string result = m_plugin->expandMacros(message, reason);
    EXPECT_EQ(result, "Temperature is 25.5 degrees");
}

// Test macro substitution with numeric values
TEST_F(DeliveryPluginExpandMacrosTest, NumericMacroSubstitution)
{
    string message = "Value is $value$ and count is $count$";
    string reason = "{\"reason\": \"triggered\", \"auditCode\": \"test\", \"data\": { \"example\" : {\"value\": 42.5, \"count\": 100}}}";
    
    string result = m_plugin->expandMacros(message, reason);
    EXPECT_EQ(result, "Value is 42.5 and count is 100");
}

// Test macro with default value when key doesn't exist
TEST_F(DeliveryPluginExpandMacrosTest, MacroWithDefaultValue)
{
    string message = "Temperature is $temperature|unknown$ degrees";
    string reason = "{\"reason\": \"triggered\", \"auditCode\": \"test\", \"data\": { \"example\" : {\"humidity\": \"60\", \"pressure\": \"1013\"}}}";
    
    string result = m_plugin->expandMacros(message, reason);
    EXPECT_EQ(result, "Temperature is unknown degrees");
}

// Test multiple macros in same message
TEST_F(DeliveryPluginExpandMacrosTest, MultipleMacros)
{
    string message = "Asset: $asset$, Value: $value$, Status: $status|normal$";
    string reason = "{\"reason\": \"triggered\", \"auditCode\": \"test\", \"data\": { \"example\" : {\"asset\": \"sensor1\", \"value\": 75.2}}}";
    
    string result = m_plugin->expandMacros(message, reason);
    EXPECT_EQ(result, "Asset: sensor1, Value: 75.2, Status: normal");
}

// Test invalid JSON in reason
TEST_F(DeliveryPluginExpandMacrosTest, InvalidJSON)
{
    string message = "Value: $value$";
    string reason = "{\"data\": {\"value\": \"test\"}"; // Missing closing brace
    
    string result = m_plugin->expandMacros(message, reason);
    EXPECT_EQ(result, message); // Should return original message
}

// Test missing data element in reason
TEST_F(DeliveryPluginExpandMacrosTest, MissingDataElement)
{
    string message = "Value: $value$";
    string reason = "{\"other\": {\"value\": \"test\"}}";
    
    string result = m_plugin->expandMacros(message, reason);
    EXPECT_EQ(result, message); // Should return original message
}

// Test message without macros
TEST_F(DeliveryPluginExpandMacrosTest, NoMacros)
{
    string message = "This is a simple message without macros";
    string reason = "{\"reason\": \"triggered\", \"auditCode\": \"test\", \"data\": {\"value\": \"test\"}}";
    
    string result = m_plugin->expandMacros(message, reason);
    EXPECT_EQ(result, message);
}

// Test macro without default when key doesn't exist
TEST_F(DeliveryPluginExpandMacrosTest, MacroWithoutDefault)
{
    string message = "Value: $nonexistent$";
    string reason = "{\"reason\": \"triggered\", \"auditCode\": \"test\", \"data\": { \"example\" : {\"existing\": \"value\"}}";
    
    string result = m_plugin->expandMacros(message, reason);
    EXPECT_EQ(result, "Value: $nonexistent$");
}

// Test different numeric types
TEST_F(DeliveryPluginExpandMacrosTest, DifferentNumericTypes)
{
    string message = "Int: $int$, Double: $double$";
    string reason = "{\"reason\": \"triggered\", \"auditCode\": \"test\", \"data\": { \"example\" : {\"int\": 42, \"double\": 3.14159}}}";
    
    string result = m_plugin->expandMacros(message, reason);
    EXPECT_EQ(result, "Int: 42, Double: 3.14159");
}

// Test empty message
TEST_F(DeliveryPluginExpandMacrosTest, EmptyMessage)
{
    string message = "";
    string reason = "{\"reason\": \"triggered\", \"auditCode\": \"test\", \"data\": { \"example\" : {\"value\": \"test\"}}}";
    
    string result = m_plugin->expandMacros(message, reason);
    EXPECT_EQ(result, "");
}

// Test that demonstrates the bug in the original implementation
// This test would crash if the data object is empty
TEST_F(DeliveryPluginExpandMacrosTest, EmptyDataObjectBug)
{
    string message = "Value: $test$";
    string reason = "{\"reason\": \"triggered\", \"auditCode\": \"test\", \"data\": {}}";
    
    string result = m_plugin->expandMacros(message, reason);
    EXPECT_EQ(result, "Value: $test$");
}
