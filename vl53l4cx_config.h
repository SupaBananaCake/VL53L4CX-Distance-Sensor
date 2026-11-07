#pragma once

#ifndef VL53L4CX_I2C_PORT
#define VL53L4CX_I2C_PORT 0
#endif

#ifndef VL53L4CX_SDA_PIN
#define VL53L4CX_SDA_PIN 4
#endif

#ifndef VL53L4CX_SCL_PIN
#define VL53L4CX_SCL_PIN 5
#endif

#ifndef VL53L4CX_XSHUT_PIN
#define VL53L4CX_XSHUT_PIN -1
#endif

#define VL53L4CX_I2C_ADDRESS 0x29
#define VL53L4CX_I2C_SPEED_KHZ 400
#define VL53L4CX_I2C_TIMEOUT_US 50000
#define VL53L4CX_MAX_TRANSFER_SIZE 256
#define VL53L4CX_TIMING_BUDGET_US 50000

#if VL53L4CX_I2C_PORT < 0 || VL53L4CX_I2C_PORT > 1
#error "I2C port must be 0 or 1"
#endif

#if VL53L4CX_SDA_PIN < 0 || VL53L4CX_SDA_PIN > 29 || VL53L4CX_SDA_PIN % 4 != VL53L4CX_I2C_PORT * 2
#error "SDA pin does not match the selected I2C port"
#endif

#if VL53L4CX_SCL_PIN < 0 || VL53L4CX_SCL_PIN > 29 || VL53L4CX_SCL_PIN % 4 != VL53L4CX_I2C_PORT * 2 + 1
#error "SCL pin does not match the selected I2C port"
#endif

#if VL53L4CX_XSHUT_PIN < -1 || VL53L4CX_XSHUT_PIN > 29 || VL53L4CX_XSHUT_PIN == VL53L4CX_SDA_PIN || VL53L4CX_XSHUT_PIN == VL53L4CX_SCL_PIN
#error "XSHUT must be -1 or a separate GPIO pin"
#endif
