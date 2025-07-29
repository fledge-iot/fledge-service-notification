# Notification Queue Unit Tests

## Overview

This document describes the comprehensive unit test suite for the `notification_queue.cpp` file, which implements the notification queue system for the Fledge notification service.

## Test Structure

### Test Classes Covered

1. **NotificationDataElement** - Represents notification data stored in per-rule buffers
2. **NotificationQueueElement** - Represents items stored in the queue
3. **NotificationQueue** - Main queue management class
4. **ResultData** - Keeps result data for datapoint operations
5. **AssetData** - Keeps string results and reading data for asset evaluation

### Mock Classes

- **MockNotificationRule** - Mock implementation of NotificationRule
- **MockNotificationInstance** - Mock implementation of NotificationInstance  
- **MockNotificationManager** - Mock implementation of NotificationManager

## Test Categories

### 1. Constructor and Destructor Tests
- **NotificationDataElementConstructor** - Tests proper initialization
- **NotificationDataElementDestructor** - Tests memory cleanup
- **NotificationQueueElementConstructor** - Tests queue element creation
- **NotificationQueueElementDestructor** - Tests queue element cleanup
- **NotificationQueueConstructor** - Tests queue initialization
- **NotificationQueueDestructor** - Tests queue cleanup

### 2. Basic Functionality Tests
- **NotificationQueueElementQueuedTimeCheck** - Tests time checking functionality
- **NotificationQueueAddElement** - Tests adding elements to queue
- **NotificationQueueAddElementWhenStopped** - Tests behavior when queue is stopped

### 3. Buffer Operations Tests
- **FeedDataBuffer** - Tests feeding data into buffers
- **FeedDataBufferWithNullData** - Tests null data handling
- **GetBufferData** - Tests retrieving buffer data
- **ClearBufferData** - Tests clearing buffer data
- **KeepBufferData** - Tests keeping specific amount of buffer data

### 4. Data Processing Tests
- **ProcessDataSet** - Tests processing individual data sets
- **FeedAllDataBuffers** - Tests feeding all data buffers
- **FeedAllDataBuffersWithNullData** - Tests null data handling

### 5. Evaluation Methods Tests
- **SetValue** - Tests setting datapoint values
- **SetMinValue** - Tests minimum value calculation
- **SetMaxValue** - Tests maximum value calculation
- **SetSumValues** - Tests sum calculation for averages
- **SetLatestValue** - Tests setting latest values

### 6. Aggregation Tests
- **AggregateData** - Tests data aggregation functionality
- **SetSingleItemData** - Tests single item data processing
- **ProcessAllReadings** - Tests processing all readings
- **ProcessAllBuffers** - Tests processing all buffers
- **ProcessDataBuffer** - Tests processing individual data buffers

### 7. Advanced Processing Tests
- **ProcessAllDataBuffers** - Tests processing all data buffers
- **SendNotification** - Tests notification sending
- **EvalRule** - Tests rule evaluation

### 8. Thread Safety Tests
- **ThreadSafety** - Tests multi-threaded operations

### 9. Edge Cases Tests
- **EmptyReadingSet** - Tests handling empty reading sets
- **MultipleAssets** - Tests multiple asset handling
- **LargeDataSet** - Tests large data set processing
- **DifferentDataTypes** - Tests different data type handling

### 10. Time-Based Processing Tests
- **ProcessTime** - Tests time-based processing

### 11. Queue Management Tests
- **QueueStop** - Tests queue stopping functionality
- **SingletonPattern** - Tests singleton pattern implementation

### 12. Memory Management Tests
- **MemoryManagement** - Tests memory allocation and cleanup

### 13. Error Handling Tests
- **NullDataHandling** - Tests null data handling
- **EmptyAssetName** - Tests empty asset name handling
- **EmptyRuleName** - Tests empty rule name handling

### 14. Performance Tests
- **PerformanceTest** - Tests performance with large datasets

## Test Coverage

### Core Functionality Coverage
- ✅ Queue element creation and destruction
- ✅ Buffer management operations
- ✅ Data processing and aggregation
- ✅ Evaluation methods (Min, Max, Average, All)
- ✅ Thread safety and synchronization
- ✅ Memory management
- ✅ Error handling

### Edge Cases Coverage
- ✅ Null data handling
- ✅ Empty datasets
- ✅ Large datasets
- ✅ Multiple assets
- ✅ Different data types
- ✅ Time-based processing

### Performance Coverage
- ✅ High-volume data processing
- ✅ Memory usage patterns
- ✅ Thread safety under load

## Running the Tests

### Prerequisites
- Google Test framework
- Fledge notification service dependencies
- CMake build system

### Build Commands
```bash
cd tests/unit/C/services/notification/build
make clean
make
```

### Run Commands
```bash
# Run all notification queue tests
./RunTests --gtest_filter="NotificationQueueTest.*"

# Run specific test categories
./RunTests --gtest_filter="NotificationQueueTest.*Constructor*"
./RunTests --gtest_filter="NotificationQueueTest.*Buffer*"
./RunTests --gtest_filter="NotificationQueueTest.*Thread*"
```

## Expected Output

### Successful Test Run
```
[==========] Running 45 tests from 1 test suite.
[----------] Global test environment set-up.
[----------] 45 tests from NotificationQueueTest
[ RUN      ] NotificationQueueTest.NotificationDataElementConstructor
[       OK ] NotificationQueueTest.NotificationDataElementConstructor (0 ms)
[ RUN      ] NotificationQueueTest.NotificationDataElementDestructor
[       OK ] NotificationQueueTest.NotificationDataElementDestructor (0 ms)
...
[----------] 45 tests from NotificationQueueTest (123 ms total)

[----------] Global test environment tear-down
[==========] 45 tests from 1 test suite ran. (125 ms total)
[  PASSED  ] 45 tests.
```

### Test Categories Breakdown
- **Constructor/Destructor Tests**: 6 tests
- **Basic Functionality Tests**: 3 tests
- **Buffer Operations Tests**: 5 tests
- **Data Processing Tests**: 3 tests
- **Evaluation Methods Tests**: 5 tests
- **Aggregation Tests**: 5 tests
- **Advanced Processing Tests**: 3 tests
- **Thread Safety Tests**: 1 test
- **Edge Cases Tests**: 4 tests
- **Time-Based Processing Tests**: 1 test
- **Queue Management Tests**: 2 tests
- **Memory Management Tests**: 1 test
- **Error Handling Tests**: 3 tests
- **Performance Tests**: 1 test

## Key Features Tested

### 1. Queue Management
- Element addition and removal
- Queue state management
- Thread-safe operations

### 2. Buffer Operations
- Data feeding into buffers
- Buffer data retrieval
- Buffer clearing and maintenance
- Per-rule buffer management

### 3. Data Processing
- Reading set processing
- Datapoint aggregation
- Evaluation type handling (Min, Max, Average, All)
- Single item and interval processing

### 4. Evaluation Methods
- Minimum value calculation
- Maximum value calculation
- Sum calculation for averages
- Latest value tracking

### 5. Thread Safety
- Multi-threaded data access
- Synchronization mechanisms
- Race condition prevention

### 6. Memory Management
- Proper allocation and deallocation
- Memory leak prevention
- Resource cleanup

### 7. Error Handling
- Null pointer handling
- Empty data handling
- Invalid input handling

## Dependencies

### Required Headers
- `notification_queue.h`
- `notification_manager.h`
- `notification_subscription.h`
- `reading_set.h`
- `reading.h`
- `datapoint.h`
- `logger.h`

### Mock Dependencies
- MockNotificationRule
- MockNotificationInstance
- MockNotificationManager

## Notes

### Known Limitations
1. Some tests may require specific Fledge environment setup
2. Time-based tests may have timing dependencies
3. Performance tests may vary based on system resources

### Future Enhancements
1. Add more comprehensive time-based rule testing
2. Include delivery plugin integration tests
3. Add more complex notification rule scenarios
4. Enhance performance benchmarking

## Maintenance

### Adding New Tests
1. Follow the existing test naming convention
2. Use the NotificationQueueTest fixture
3. Include proper setup and teardown
4. Add documentation for new test categories

### Updating Tests
1. Update this README when adding new test categories
2. Maintain test coverage documentation
3. Update expected output examples
4. Review and update mock classes as needed 