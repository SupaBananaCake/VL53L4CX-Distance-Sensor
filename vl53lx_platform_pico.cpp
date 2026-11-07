#include <cstdint>
#include <cstring>

#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "vl53l4cx_config.h"
#include "vl53lx_platform.h"

namespace {

i2c_inst_t *sensor_i2c() {
    return VL53L4CX_I2C_PORT == 0 ? i2c0 : i2c1;
}

bool valid_device(const VL53LX_Dev_t *device) {
    return device != nullptr &&
           (device->i2c_slave_address & 1u) == 0 &&
           (device->i2c_slave_address >> 1) >= 0x08 &&
           (device->i2c_slave_address >> 1) <= 0x77;
}

VL53LX_Error transfer_status(VL53LX_Dev_t *device, int actual, uint32_t expected) {
    if (actual == static_cast<int>(expected)) {
        return VL53LX_ERROR_NONE;
    }

    i2c_deinit(sensor_i2c());
    i2c_init(sensor_i2c(), static_cast<uint32_t>(device->comms_speed_khz) * 1000u);
    return actual == PICO_ERROR_TIMEOUT ? VL53LX_ERROR_TIME_OUT
                                       : VL53LX_ERROR_CONTROL_INTERFACE;
}

}

VL53LX_Error VL53LX_CommsInitialise(VL53LX_Dev_t *device, uint8_t type, uint16_t speed_khz) {
    if (!valid_device(device) || type != 1 || speed_khz == 0 || speed_khz > 400) {
        return VL53LX_ERROR_INVALID_PARAMS;
    }

    device->comms_type = type;
    device->comms_speed_khz = speed_khz;
    i2c_init(sensor_i2c(), static_cast<uint32_t>(speed_khz) * 1000u);
    gpio_set_function(VL53L4CX_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(VL53L4CX_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(VL53L4CX_SDA_PIN);
    gpio_pull_up(VL53L4CX_SCL_PIN);
    return VL53LX_ERROR_NONE;
}

VL53LX_Error VL53LX_CommsClose(VL53LX_Dev_t *device) {
    if (device == nullptr) {
        return VL53LX_ERROR_INVALID_PARAMS;
    }
    i2c_deinit(sensor_i2c());
    return VL53LX_ERROR_NONE;
}

VL53LX_Error VL53LX_WriteMulti(VL53LX_Dev_t *device, uint16_t index,
                               uint8_t *data, uint32_t count) {
    if (!valid_device(device) || data == nullptr || count == 0 ||
        count > VL53L4CX_MAX_TRANSFER_SIZE || count > 0x10000u - index) {
        return VL53LX_ERROR_INVALID_PARAMS;
    }

    uint8_t buffer[VL53L4CX_MAX_TRANSFER_SIZE + 2];
    buffer[0] = static_cast<uint8_t>(index >> 8);
    buffer[1] = static_cast<uint8_t>(index);
    std::memcpy(buffer + 2, data, count);

    const int written = i2c_write_timeout_us(sensor_i2c(), device->i2c_slave_address >> 1,
                                             buffer, count + 2, false,
                                             VL53L4CX_I2C_TIMEOUT_US);
    return transfer_status(device, written, count + 2);
}

VL53LX_Error VL53LX_ReadMulti(VL53LX_Dev_t *device, uint16_t index,
                              uint8_t *data, uint32_t count) {
    if (!valid_device(device) || data == nullptr || count == 0 ||
        count > VL53L4CX_MAX_TRANSFER_SIZE || count > 0x10000u - index) {
        return VL53LX_ERROR_INVALID_PARAMS;
    }

    std::memset(data, 0, count);
    const uint8_t address[2] = {static_cast<uint8_t>(index >> 8), static_cast<uint8_t>(index)};
    const int written = i2c_write_timeout_us(sensor_i2c(), device->i2c_slave_address >> 1,
                                             address, sizeof(address), true,
                                             VL53L4CX_I2C_TIMEOUT_US);
    VL53LX_Error status = transfer_status(device, written, sizeof(address));
    if (status != VL53LX_ERROR_NONE) {
        return status;
    }

    const int received = i2c_read_timeout_us(sensor_i2c(), device->i2c_slave_address >> 1,
                                             data, count, false,
                                             VL53L4CX_I2C_TIMEOUT_US);
    status = transfer_status(device, received, count);
    if (status != VL53LX_ERROR_NONE) {
        std::memset(data, 0, count);
    }
    return status;
}

VL53LX_Error VL53LX_WrByte(VL53LX_Dev_t *device, uint16_t index, uint8_t value) {
    return VL53LX_WriteMulti(device, index, &value, 1);
}

VL53LX_Error VL53LX_WrWord(VL53LX_Dev_t *device, uint16_t index, uint16_t value) {
    uint8_t data[2] = {static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value)};
    return VL53LX_WriteMulti(device, index, data, sizeof(data));
}

VL53LX_Error VL53LX_WrDWord(VL53LX_Dev_t *device, uint16_t index, uint32_t value) {
    uint8_t data[4] = {static_cast<uint8_t>(value >> 24), static_cast<uint8_t>(value >> 16),
                       static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value)};
    return VL53LX_WriteMulti(device, index, data, sizeof(data));
}

VL53LX_Error VL53LX_RdByte(VL53LX_Dev_t *device, uint16_t index, uint8_t *value) {
    if (value == nullptr) {
        return VL53LX_ERROR_INVALID_PARAMS;
    }
    *value = 0;
    return VL53LX_ReadMulti(device, index, value, 1);
}

VL53LX_Error VL53LX_RdWord(VL53LX_Dev_t *device, uint16_t index, uint16_t *value) {
    if (value == nullptr) {
        return VL53LX_ERROR_INVALID_PARAMS;
    }
    *value = 0;
    uint8_t data[2] = {};
    const VL53LX_Error status = VL53LX_ReadMulti(device, index, data, sizeof(data));
    if (status == VL53LX_ERROR_NONE) {
        *value = static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8) | data[1]);
    }
    return status;
}

VL53LX_Error VL53LX_RdDWord(VL53LX_Dev_t *device, uint16_t index, uint32_t *value) {
    if (value == nullptr) {
        return VL53LX_ERROR_INVALID_PARAMS;
    }
    *value = 0;
    uint8_t data[4] = {};
    const VL53LX_Error status = VL53LX_ReadMulti(device, index, data, sizeof(data));
    if (status == VL53LX_ERROR_NONE) {
        *value = (static_cast<uint32_t>(data[0]) << 24) | (static_cast<uint32_t>(data[1]) << 16) |
                 (static_cast<uint32_t>(data[2]) << 8) | data[3];
    }
    return status;
}

VL53LX_Error VL53LX_WaitUs(VL53LX_Dev_t *, int32_t duration) {
    if (duration < 0) {
        return VL53LX_ERROR_INVALID_PARAMS;
    }
    sleep_us(static_cast<uint64_t>(duration));
    return VL53LX_ERROR_NONE;
}

VL53LX_Error VL53LX_WaitMs(VL53LX_Dev_t *, int32_t duration) {
    if (duration < 0) {
        return VL53LX_ERROR_INVALID_PARAMS;
    }
    sleep_ms(static_cast<uint32_t>(duration));
    return VL53LX_ERROR_NONE;
}

VL53LX_Error VL53LX_GetTimerFrequency(int32_t *frequency) {
    if (frequency == nullptr) {
        return VL53LX_ERROR_INVALID_PARAMS;
    }
    *frequency = 1000;
    return VL53LX_ERROR_NONE;
}

VL53LX_Error VL53LX_GetTimerValue(int32_t *value) {
    if (value == nullptr) {
        return VL53LX_ERROR_INVALID_PARAMS;
    }
    *value = static_cast<int32_t>(to_ms_since_boot(get_absolute_time()));
    return VL53LX_ERROR_NONE;
}

VL53LX_Error VL53LX_GetTickCount(VL53LX_Dev_t *, uint32_t *value) {
    if (value == nullptr) {
        return VL53LX_ERROR_INVALID_PARAMS;
    }
    *value = to_ms_since_boot(get_absolute_time());
    return VL53LX_ERROR_NONE;
}

VL53LX_Error VL53LX_WaitValueMaskEx(VL53LX_Dev_t *device, uint32_t timeout_ms,
                                    uint16_t index, uint8_t value, uint8_t mask,
                                    uint32_t poll_delay_ms) {
    if (!valid_device(device)) {
        return VL53LX_ERROR_INVALID_PARAMS;
    }

    const uint32_t start = to_ms_since_boot(get_absolute_time());
    device->new_data_ready_poll_duration_ms = 0;

    while (true) {
        uint8_t current = 0;
        const VL53LX_Error status = VL53LX_RdByte(device, index, &current);
        const uint32_t elapsed = to_ms_since_boot(get_absolute_time()) - start;
        device->new_data_ready_poll_duration_ms = elapsed;
        if (status != VL53LX_ERROR_NONE) {
            return status;
        }
        if ((current & mask) == value) {
            return VL53LX_ERROR_NONE;
        }
        if (elapsed >= timeout_ms) {
            return VL53LX_ERROR_TIME_OUT;
        }

        const uint32_t remaining = timeout_ms - elapsed;
        const uint32_t delay = poll_delay_ms == 0 ? 1 : poll_delay_ms;
        sleep_ms(delay < remaining ? delay : remaining);
    }
}
