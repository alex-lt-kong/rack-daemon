#ifndef SEVEN_SEG_DISPLAY_H
#define SEVEN_SEG_DISPLAY_H

#include <pigpio.h>

#include <stdbool.h>
#include <stdint.h>

#define DIGIT_COUNT 8

void push_bit(bool bit);
bool get_bit(uint16_t value, int n);

void init_7seg_display();

void write_data_to_register(uint16_t value);

int show(uint8_t *values, bool *with_dots);

#endif /* SEVEN_SEG_DISPLAY_H */
