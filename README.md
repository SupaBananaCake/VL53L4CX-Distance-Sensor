# VL53L4CX distance sensor

Read distances from a VL53L4CX with a Raspberry Pi Pico 2 W and see the results over USB:

```text
Target 1: 247 mm
```

The program reads continuously, reports invalid readings, and retries if the sensor stops responding.

## Connect the sensor

For a breakout that accepts 3.3 V power:

| Sensor | Pico 2 W |
| --- | --- |
| VIN | 3V3(OUT), pin 36 |
| GND | GND, pin 38 |
| SDA | GP4, pin 6 |
| SCL | GP5, pin 7 |
| XSHUT | Leave unconnected if the breakout pulls it high |
| GPIO1 / INT | Leave unconnected |

Use the breakout's I2C pull-ups, or add suitable pull-ups if it has none. I2C signals must be compatible with the Pico's 3.3 V GPIO.

To change pins, edit [vl53l4cx_config.h](vl53l4cx_config.h). If your breakout needs XSHUT control, connect it to a spare GPIO and set `VL53L4CX_XSHUT_PIN` to that GPIO number. The default I2C address is `0x29`.

## Build and run

Install the Pico SDK, ARM toolchain, CMake, and Ninja, then run:

```sh
cmake -S . -B build -G Ninja
cmake --build build --parallel 4
```

1. Hold **BOOTSEL** while plugging the Pico into USB.
2. Copy `build/VL53L4CX-Distance-Sensor.uf2` onto the drive that appears.
3. Open the Pico's USB serial port in a terminal. Use 115200 baud and enable DTR.

Distances are printed in millimetres. You'll also see `No target detected` or an invalid-reading status when a usable distance isn't available. If connection errors keep appearing, check power, SDA/SCL, and XSHUT.

## Working on the code

[VL53L4CX-Distance-Sensor.cpp](VL53L4CX-Distance-Sensor.cpp) runs the application. [vl53lx_platform_pico.cpp](vl53lx_platform_pico.cpp) handles I2C communication. Custom code and tests use C++17; the supplied ST driver lives in `drivers/`.

To run the tests on your computer, use a native C++ compiler environment, such as an x64 Visual Studio developer prompt on Windows:

```sh
cmake -S tests -B build/host-ninja -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/host-ninja
ctest --test-dir build/host-ninja --output-on-failure
```

The tests check communication and error handling with simulated hardware. Real sensor operation still needs to be checked on the board.

For calibration and sensor details, see [ST's driver guide](https://www.st.com/resource/en/user_manual/um2923-a-guide-to-using-the-vl53l4cx-timeofflight-sensor-with-extended-distance-measurement-stmicroelectronics.pdf).
