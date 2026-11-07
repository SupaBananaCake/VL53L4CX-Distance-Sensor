#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "vl53l4cx_config.h"
#include "vl53lx_platform.h"

#define CHECK(condition) do { if (!(condition)) { std::fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #condition); std::exit(1); } } while (0)

i2c_inst_t mock_i2c0 = {};
i2c_inst_t mock_i2c1 = {1};
static VL53LX_Dev_t device;
static uint64_t now_us;
static int write_result = INT_MAX;
static int read_result = INT_MAX;
static uint8_t written[258];
static size_t written_count;
static bool write_nostop;
static unsigned reads;
static unsigned writes;
static unsigned resets;
static unsigned ready_on_read;
static uint8_t response[256];

unsigned i2c_init(i2c_inst_t *port, unsigned speed) {
    CHECK(port == i2c0);
    CHECK(speed == 400000);
    return speed;
}

void i2c_deinit(i2c_inst_t *port) {
    CHECK(port == i2c0);
    ++resets;
}

void gpio_set_function(unsigned pin, unsigned function) {
    CHECK(pin == 4 || pin == 5);
    CHECK(function == GPIO_FUNC_I2C);
}

void gpio_pull_up(unsigned pin) {
    CHECK(pin == 4 || pin == 5);
}

void sleep_us(uint64_t duration) { now_us += duration; }
void sleep_ms(uint32_t duration) { now_us += static_cast<uint64_t>(duration) * 1000; }
absolute_time_t get_absolute_time() { return now_us; }
uint32_t to_ms_since_boot(absolute_time_t time) { return static_cast<uint32_t>(time / 1000); }

int i2c_write_timeout_us(i2c_inst_t *port, uint8_t address, const uint8_t *data,
                         size_t count, bool nostop, unsigned timeout) {
    CHECK(port == i2c0);
    CHECK(address == 0x29);
    CHECK(timeout == VL53L4CX_I2C_TIMEOUT_US);
    CHECK(count <= sizeof(written));
    std::memcpy(written, data, count);
    written_count = count;
    write_nostop = nostop;
    ++writes;
    return write_result == INT_MAX ? static_cast<int>(count) : write_result;
}

int i2c_read_timeout_us(i2c_inst_t *port, uint8_t address, uint8_t *data,
                        size_t count, bool nostop, unsigned timeout) {
    CHECK(port == i2c0);
    CHECK(address == 0x29);
    CHECK(timeout == VL53L4CX_I2C_TIMEOUT_US);
    CHECK(!nostop);
    CHECK(write_nostop);
    CHECK(written_count == 2);
    CHECK(count <= sizeof(response));
    ++reads;
    std::memcpy(data, response, count);
    if (ready_on_read != 0) {
        data[0] = reads >= ready_on_read ? 0x81 : 0x80;
    }
    return read_result == INT_MAX ? static_cast<int>(count) : read_result;
}

static void test_write_order() {
    const uint8_t expected_word[] = {0x12, 0x34, 0xab, 0xcd};
    const uint8_t expected_dword[] = {0x56, 0x78, 0x89, 0xab, 0xcd, 0xef};
    CHECK(VL53LX_WrWord(&device, 0x1234, 0xabcd) == VL53LX_ERROR_NONE);
    CHECK(written_count == sizeof(expected_word));
    CHECK(std::memcmp(written, expected_word, sizeof(expected_word)) == 0);
    CHECK(!write_nostop);
    CHECK(VL53LX_WrDWord(&device, 0x5678, 0x89abcdef) == VL53LX_ERROR_NONE);
    CHECK(written_count == sizeof(expected_dword));
    CHECK(std::memcmp(written, expected_dword, sizeof(expected_dword)) == 0);
}

static void test_read_order() {
    uint16_t word = 0;
    uint32_t dword = 0;
    response[0] = 0xeb;
    response[1] = 0xaa;
    CHECK(VL53LX_RdWord(&device, 0x010f, &word) == VL53LX_ERROR_NONE);
    CHECK(word == 0xebaa);
    CHECK(written[0] == 0x01 && written[1] == 0x0f);
    response[0] = 0x89;
    response[1] = 0xab;
    response[2] = 0xcd;
    response[3] = 0xef;
    CHECK(VL53LX_RdDWord(&device, 0x1234, &dword) == VL53LX_ERROR_NONE);
    CHECK(dword == 0x89abcdef);
}

static void test_short_write() {
    uint16_t word = 123;
    write_result = 1;
    CHECK(VL53LX_RdWord(&device, 0x010f, &word) == VL53LX_ERROR_CONTROL_INTERFACE);
    CHECK(reads == 0);
    CHECK(word == 0);
    CHECK(resets == 1);
    CHECK(VL53LX_WrWord(&device, 0x1234, 0xabcd) == VL53LX_ERROR_CONTROL_INTERFACE);
}

static void test_short_read() {
    uint8_t data[4] = {1, 2, 3, 4};
    std::memset(response, 0xff, sizeof(response));
    read_result = 2;
    CHECK(VL53LX_ReadMulti(&device, 0x010f, data, 4) == VL53LX_ERROR_CONTROL_INTERFACE);
    CHECK(data[0] == 0 && data[1] == 0 && data[2] == 0 && data[3] == 0);
    CHECK(resets == 1);
}

static void test_transport_errors() {
    uint8_t value = 0xff;
    write_result = PICO_ERROR_TIMEOUT;
    CHECK(VL53LX_RdByte(&device, 0x010f, &value) == VL53LX_ERROR_TIME_OUT);
    CHECK(value == 0 && reads == 0);
    write_result = -1;
    CHECK(VL53LX_WrByte(&device, 0x010f, 0) == VL53LX_ERROR_CONTROL_INTERFACE);
    write_result = INT_MAX;
    read_result = PICO_ERROR_TIMEOUT;
    CHECK(VL53LX_RdByte(&device, 0x010f, &value) == VL53LX_ERROR_TIME_OUT);
    CHECK(value == 0);
    CHECK(VL53LX_WaitValueMaskEx(&device, 100, 0x0006, 1, 1, 1) == VL53LX_ERROR_TIME_OUT);
    CHECK(reads == 2);
    CHECK(resets == 4);
}

static void test_invalid_arguments() {
    uint8_t data[257] = {};
    CHECK(VL53LX_WriteMulti(&device, 0, data, 257) == VL53LX_ERROR_INVALID_PARAMS);
    CHECK(VL53LX_WriteMulti(&device, 0, data, UINT32_MAX) == VL53LX_ERROR_INVALID_PARAMS);
    CHECK(VL53LX_WriteMulti(&device, 0xffff, data, 2) == VL53LX_ERROR_INVALID_PARAMS);
    CHECK(VL53LX_ReadMulti(&device, 0, data, 0) == VL53LX_ERROR_INVALID_PARAMS);
    CHECK(VL53LX_ReadMulti(&device, 0, nullptr, 1) == VL53LX_ERROR_INVALID_PARAMS);
    CHECK(VL53LX_RdWord(&device, 0, nullptr) == VL53LX_ERROR_INVALID_PARAMS);
    CHECK(VL53LX_WrByte(nullptr, 0, 0) == VL53LX_ERROR_INVALID_PARAMS);
    CHECK(VL53LX_CommsInitialise(&device, 0, 400) == VL53LX_ERROR_INVALID_PARAMS);
    CHECK(VL53LX_CommsInitialise(&device, 1, 0) == VL53LX_ERROR_INVALID_PARAMS);
    device.i2c_slave_address = 0x29;
    CHECK(VL53LX_WrByte(&device, 0, 0) == VL53LX_ERROR_INVALID_PARAMS);
    CHECK(writes == 0 && reads == 0);
}

static void test_poll_ready() {
    ready_on_read = 3;
    CHECK(VL53LX_WaitValueMaskEx(&device, 10, 0x0006, 1, 1, 1) == VL53LX_ERROR_NONE);
    CHECK(reads == 3);
    CHECK(device.new_data_ready_poll_duration_ms == 2);
}

static void test_poll_timeout() {
    now_us = (static_cast<uint64_t>(UINT32_MAX) - 2) * 1000;
    CHECK(VL53LX_WaitValueMaskEx(&device, 5, 0x0006, 1, 1, 0) == VL53LX_ERROR_TIME_OUT);
    CHECK(device.new_data_ready_poll_duration_ms == 5);
    CHECK(reads == 6);
    reads = 0;
    CHECK(VL53LX_WaitValueMaskEx(&device, 0, 0x0006, 1, 1, 100) == VL53LX_ERROR_TIME_OUT);
    CHECK(reads == 1);
}

static void test_timer() {
    int32_t frequency = 0;
    int32_t value = 0;
    CHECK(VL53LX_GetTimerFrequency(&frequency) == VL53LX_ERROR_NONE);
    CHECK(frequency == 1000);
    CHECK(VL53LX_WaitMs(&device, 3) == VL53LX_ERROR_NONE);
    CHECK(VL53LX_WaitUs(&device, 1000) == VL53LX_ERROR_NONE);
    CHECK(VL53LX_GetTimerValue(&value) == VL53LX_ERROR_NONE);
    CHECK(value == 4);
    CHECK(VL53LX_WaitMs(&device, -1) == VL53LX_ERROR_INVALID_PARAMS);
    CHECK(VL53LX_WaitUs(&device, -1) == VL53LX_ERROR_INVALID_PARAMS);
}

int main(int argc, char **argv) {
    const struct { const char *name; void (*run)(); } tests[] = {
        {"write_order", test_write_order},
        {"read_order", test_read_order},
        {"short_write", test_short_write},
        {"short_read", test_short_read},
        {"transport_errors", test_transport_errors},
        {"invalid_arguments", test_invalid_arguments},
        {"poll_ready", test_poll_ready},
        {"poll_timeout", test_poll_timeout},
        {"timer", test_timer}
    };
    CHECK(argc == 2);
    device.i2c_slave_address = 0x52;
    CHECK(VL53LX_CommsInitialise(&device, 1, 400) == VL53LX_ERROR_NONE);
    for (size_t index = 0; index < sizeof(tests) / sizeof(tests[0]); ++index) {
        if (std::strcmp(argv[1], tests[index].name) == 0) {
            tests[index].run();
            std::puts("PASS");
            return 0;
        }
    }
    return 1;
}
