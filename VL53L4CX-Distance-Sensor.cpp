#include <cstdint>
#include <cstdio>
#include <cstring>

#include "pico/stdio_usb.h"
#include "pico/stdlib.h"
#include "vl53l4cx_config.h"
#include "vl53lx_api.h"

namespace {

VL53LX_Dev_t sensor = {};
bool sensor_initialized = false;

bool check_status(VL53LX_Error status, const char *operation) {
    if (status == VL53LX_ERROR_NONE) {
        return true;
    }

    printf("VL53L4CX %s failed: %d\n", operation, static_cast<int>(status));
    return false;
}

bool initialize_sensor() {
    sensor_initialized = false;
    std::memset(&sensor, 0, sizeof(sensor));
    sensor.i2c_slave_address = VL53L4CX_I2C_ADDRESS << 1;

    if (!check_status(VL53LX_CommsInitialise(&sensor, 1, VL53L4CX_I2C_SPEED_KHZ),
                      "I2C initialization")) {
        return false;
    }

#if VL53L4CX_XSHUT_PIN >= 0
    gpio_init(VL53L4CX_XSHUT_PIN);
    gpio_put(VL53L4CX_XSHUT_PIN, 0);
    gpio_set_dir(VL53L4CX_XSHUT_PIN, GPIO_OUT);
    sleep_ms(2);
    gpio_put(VL53L4CX_XSHUT_PIN, 1);
#endif

    sleep_ms(5);

    if (!check_status(VL53LX_WaitDeviceBooted(&sensor), "boot")) {
        return false;
    }

    uint16_t sensor_id = 0;
    if (!check_status(VL53LX_RdWord(&sensor, 0x010f, &sensor_id), "identification")) {
        return false;
    }

    if (sensor_id != 0xebaa && sensor_id != 0xecaa) {
        printf("Unexpected sensor ID: 0x%04x; expected a VL53L4CX\n",
               static_cast<unsigned>(sensor_id));
        return false;
    }

    if (!check_status(VL53LX_DataInit(&sensor), "data initialization")) {
        return false;
    }
    sensor_initialized = true;

    if (!check_status(VL53LX_StopMeasurement(&sensor), "stop previous measurement") ||
        !check_status(VL53LX_SetDistanceMode(&sensor, VL53LX_DISTANCEMODE_MEDIUM),
                      "distance mode") ||
        !check_status(VL53LX_SetMeasurementTimingBudgetMicroSeconds(
                          &sensor, VL53L4CX_TIMING_BUDGET_US),
                      "timing budget") ||
        !check_status(VL53LX_StartMeasurement(&sensor), "start")) {
        return false;
    }

    printf("VL53L4CX ready: SDA=GP%d SCL=GP%d address=0x%02x\n",
           VL53L4CX_SDA_PIN, VL53L4CX_SCL_PIN, VL53L4CX_I2C_ADDRESS);
    return true;
}

bool read_measurement(VL53LX_MultiRangingData_t &measurement) {
    const absolute_time_t deadline = make_timeout_time_ms(2000);
    uint8_t ready = 0;

    while (!ready) {
        if (!check_status(VL53LX_GetMeasurementDataReady(&sensor, &ready), "data ready")) {
            return false;
        }
        if (ready) {
            break;
        }
        if (time_reached(deadline)) {
            return check_status(VL53LX_ERROR_TIME_OUT, "measurement");
        }
        sleep_ms(1);
    }

    if (!check_status(VL53LX_GetMultiRangingData(&sensor, &measurement), "read")) {
        return false;
    }

    if (!check_status(VL53LX_ClearInterruptAndStartMeasurement(&sensor), "restart")) {
        return false;
    }

    if (measurement.NumberOfObjectsFound > VL53LX_MAX_RANGE_RESULTS) {
        printf("VL53L4CX returned an invalid target count\n");
        return false;
    }

    return true;
}

void print_measurement(const VL53LX_MultiRangingData_t &measurement) {
    if (!stdio_usb_connected()) {
        return;
    }

    if (measurement.NumberOfObjectsFound == 0) {
        printf("No target detected\n");
        return;
    }

    for (uint8_t index = 0; index < measurement.NumberOfObjectsFound; ++index) {
        const auto &target = measurement.RangeData[index];
        if (target.RangeStatus == VL53LX_RANGESTATUS_RANGE_VALID) {
            printf("Target %u: %d mm\n", static_cast<unsigned>(index + 1),
                   static_cast<int>(target.RangeMilliMeter));
        } else {
            printf("Target %u: invalid reading (status %u)\n",
                   static_cast<unsigned>(index + 1),
                   static_cast<unsigned>(target.RangeStatus));
        }
    }
}

}

int main() {
    if (!stdio_init_all()) {
        panic("USB serial initialization failed");
    }

    const absolute_time_t usb_deadline = make_timeout_time_ms(3000);
    while (!stdio_usb_connected() && !time_reached(usb_deadline)) {
        sleep_ms(10);
    }

    while (true) {
        if (initialize_sensor()) {
            VL53LX_MultiRangingData_t measurement = {};
            while (read_measurement(measurement)) {
                print_measurement(measurement);
            }
        }

        if (sensor_initialized) {
            VL53LX_StopMeasurement(&sensor);
        }
        VL53LX_CommsClose(&sensor);
        printf("Check sensor power, SDA/SCL wiring and XSHUT; retrying in 1 second\n");
        sleep_ms(1000);
    }
}
