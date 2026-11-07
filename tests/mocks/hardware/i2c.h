#pragma once
#include <cstddef>
#include <cstdint>

struct i2c_inst_t { unsigned index; };
extern i2c_inst_t mock_i2c0;
extern i2c_inst_t mock_i2c1;
#define i2c0 (&mock_i2c0)
#define i2c1 (&mock_i2c1)

unsigned i2c_init(i2c_inst_t *port, unsigned speed);
void i2c_deinit(i2c_inst_t *port);
int i2c_write_timeout_us(i2c_inst_t *port, uint8_t address, const uint8_t *data,
                       size_t count, bool nostop, unsigned timeout);
int i2c_read_timeout_us(i2c_inst_t *port, uint8_t address, uint8_t *data,
                      size_t count, bool nostop, unsigned timeout);
