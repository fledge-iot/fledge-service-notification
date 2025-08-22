# DataAvailabilityRule Unit Tests

This document describes the comprehensive unit test suite for the `DataAvailabilityRule` class, which is a builtin notification rule in the Fledge notification service.

## Overview

The `DataAvailabilityRule` is responsible for monitoring audit log codes and generating notifications when specific audit events occur. The test suite covers all major functionality including:

- Constructor and basic properties
- Plugin information retrieval
- Initialization with various configurations
- Trigger generation
- Rule evaluation
- State management and reason reporting
- Configuration changes
- Edge cases and error handling
- Thread safety

## Test Structure

### Test Fixture: `DataAvailabilityRuleTest`

The test suite uses a Google Test fixture that provides:

- **SetUp()**: Initializes the logger for tests
- **TearDown()**: Handles cleanup
- **Helper Methods**:
  - `createBasicConfig()`: Creates standard configuration objects
  - `createTestJSON()`: Generates test JSON data with optional timestamps

## Test Categories

### 1. Basic Functionality Tests

#### Constructor and Properties
- **Constructor**: Tests object creation and basic properties
- **GetInfo**: Validates plugin information structure and values

#### Initialization Tests
- **InitWithEmptyConfig**: Tests initialization with no configuration
- **InitWithAuditCode**: Tests initialization with audit code monitoring
- **InitWithAssetCode**: Tests initialization with asset code monitoring
- **InitWithAlertsEnabled**: Tests initialization with alerts enabled
- **InitWithMultipleAuditCodes**: Tests comma-separated audit codes
- **InitWithMultipleAssetCodes**: Tests comma-separated asset codes

### 2. Trigger Generation Tests

#### Trigger JSON Generation
- **TriggersWithNoConfig**: Tests empty trigger generation
- **TriggersWithAuditCode**: Tests audit code trigger format
- **TriggersWithAssetCode**: Tests asset code trigger format
- **TriggersWithAlertsEnabled**: Tests alert trigger format
- **TriggersWithMultipleConfigurations**: Tests complex trigger combinations

### 3. Evaluation Tests

#### JSON Processing
- **EvalWithInvalidJSON**: Tests handling of malformed JSON
- **EvalWithEmptyJSON**: Tests empty JSON object handling
- **EvalWithMatchingAuditCode**: Tests successful audit code matching
- **EvalWithNonMatchingAuditCode**: Tests non-matching audit codes
- **EvalWithTimestamp**: Tests timestamp processing
- **EvalWithMultipleAuditCodes**: Tests multiple audit code evaluation

### 4. State Management Tests

#### Reason Reporting
- **ReasonWhenTriggered**: Tests reason generation when rule is triggered
- **ReasonWhenCleared**: Tests reason generation when rule is cleared
- **ReasonWithTimestamp**: Tests timestamp inclusion in reason

### 5. Configuration Management Tests

#### Dynamic Configuration
- **Reconfigure**: Tests runtime configuration changes
- **EvalAuditCode**: Tests the core audit code evaluation logic

### 6. Edge Case Tests

#### Input Validation
- **EmptyAuditCodeWithSpaces**: Tests whitespace handling
- **EmptyAssetCodeWithSpaces**: Tests asset code whitespace handling
- **MalformedAlertsConfig**: Tests invalid alert configuration
- **LongAuditCodeNames**: Tests very long audit code names
- **SpecialCharactersInAuditCodes**: Tests special character handling

### 7. Resource Management Tests

#### Cleanup and Safety
- **Shutdown**: Tests proper resource cleanup
- **PersistData**: Tests data persistence configuration

### 8. Performance and Concurrency Tests

#### Stress Testing
- **MultipleRapidEvaluations**: Tests rapid successive evaluations
- **ThreadSafetyTriggers**: Tests thread safety of trigger generation

## Test Data Examples

### Configuration Examples

```cpp
// Basic audit code configuration
ConfigCategory config = createBasicConfig("AUDIT001");

// Multiple audit codes
ConfigCategory config = createBasicConfig("AUDIT001,AUDIT002,AUDIT003");

// Asset code configuration
ConfigCategory config = createBasicConfig("", "ASSET001");

// Alerts enabled
ConfigCategory config = createBasicConfig("", "", "true");
```

### JSON Test Data Examples

```cpp
// Simple audit code JSON
string json = createTestJSON("AUDIT001");
// Result: {"AUDIT001": "test_value"}

// JSON with timestamp
string json = createTestJSON("AUDIT001", 1234567890.123);
// Result: {"AUDIT001": "test_value", "timestamp_AUDIT001": 1234567890.123}
```

## Expected Test Output

### Trigger JSON Format

```json
{
  "triggers": [
    {"asset": "ASSET001"},
    {"audit": "AUDIT001"},
    {"alert": "alert"}
  ]
}
```

### Reason JSON Format

```json
{
  "reason": "triggered",
  "auditCode": "...",
  "timestamp": "2023-01-01 12:00:00.123456+00:00"
}
```

## Running the Tests

### Prerequisites

- Google Test framework
- Fledge development environment
- CMake build system

### Build and Run

```bash
# Navigate to test directory
cd tests/unit/C/services/notification

# Build tests
mkdir build && cd build
cmake ..
make

# Run tests
./RunTests

# Run with verbose output
./RunTests --gtest_verbose
```

### Running Specific Tests

```bash
# Run only DataAvailabilityRule tests
./RunTests --gtest_filter="DataAvailabilityRuleTest*"

# Run specific test
./RunTests --gtest_filter="DataAvailabilityRuleTest.Constructor"

# Run tests matching pattern
./RunTests --gtest_filter="*Triggers*"
```

## Test Coverage

The test suite provides comprehensive coverage of:

- **Function Coverage**: All public methods are tested
- **Branch Coverage**: Both success and failure paths are tested
- **Edge Case Coverage**: Boundary conditions and error scenarios
- **Thread Safety**: Concurrent access patterns
- **Memory Management**: Resource cleanup and allocation

## Key Test Scenarios

### 1. Configuration Parsing
- Empty configurations
- Single values
- Comma-separated lists
- Invalid values
- Whitespace handling

### 2. JSON Processing
- Valid JSON parsing
- Invalid JSON handling
- Empty JSON objects
- Complex nested structures
- Timestamp processing

### 3. State Transitions
- Rule triggering
- Rule clearing
- State persistence
- Configuration changes

### 4. Error Handling
- Invalid inputs
- Missing data
- Malformed configurations
- Resource failures

## Maintenance

### Adding New Tests

When adding new functionality to `DataAvailabilityRule`:

1. Add corresponding test cases to the appropriate category
2. Follow the existing naming convention
3. Include both positive and negative test cases
4. Add edge case tests for new parameters
5. Update this documentation

### Test Maintenance

- Keep tests independent and isolated
- Use descriptive test names
- Include clear assertions with meaningful messages
- Maintain helper methods for common operations
- Update tests when the interface changes

## Troubleshooting

### Common Issues

1. **Build Failures**: Ensure all dependencies are installed
2. **Test Failures**: Check that the test environment is properly configured
3. **Memory Leaks**: Use valgrind or similar tools for memory analysis
4. **Thread Issues**: Run with thread sanitizer for concurrency problems

### Debugging Tests

```bash
# Run with debug output
./RunTests --gtest_verbose --gtest_break_on_failure

# Run specific failing test
./RunTests --gtest_filter="DataAvailabilityRuleTest.EvalWithInvalidJSON"
```

## Contributing

When contributing to the test suite:

1. Follow the existing code style
2. Add comprehensive documentation
3. Ensure tests are deterministic
4. Include both unit and integration test scenarios
5. Maintain backward compatibility

## References

- [Google Test Documentation](https://github.com/google/googletest)
- [Fledge Notification Service Documentation](https://fledge-iot.readthedocs.io/)
- [C++ Unit Testing Best Practices](https://github.com/google/googletest/blob/master/googletest/docs/primer.md) 