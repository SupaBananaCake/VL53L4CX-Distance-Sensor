#pragma once
#include <cstdint>

#define GPIO_FUNC_I2C 3
#define PICO_ERROR_TIMEOUT -2
using absolute_time_t = uint64_t;

void gpio_set_function(unsigned pin, unsigned function);
void gpio_pull_up(unsigned pin);
void sleep_us(uint64_t duration);
void sleep_ms(uint32_t duration);
absolute_time_t get_absolute_time(void);
uint32_t to_ms_since_boot(absolute_time_t time);
