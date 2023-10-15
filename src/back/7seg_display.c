// This is a brute-force translation of
// https://github.com/shrikantpatnaik/Pi7SegPy/blob/master/Pi7SegPy.py

#include "7seg_display.h"

#include <stdio.h>
#include <unistd.h>

const int data = 17;
const int clk = 11;
const int latch = 18;
const int chain = 2;
/*
The 595 has two registers, each with just 8 bits of data. The first one is
called the Shift Register.

Whenever we apply a clock pulse to a 595, two things happen:
  * The bits in the Shift Register move one step to the left. For example, Bit 7
accepts the value that was previously in bit 6, bit 6 gets the value of bit 5
etc.
  * Bit 0 in the Shift Register accepts the current value on DATA pin.

On enabling the Latch pin, the contents of Shift Register are copied into the
second register, called the Storage/Latch Register. Each bit of the Storage
Register is connected to one of the output pins QA–QH of the IC, so in general,
when the value in the Storage Register changes, so do the outputs.
*/

void push_bit(bool bit) {
  gpioWrite(clk, PI_LOW);
  gpioWrite(data, bit);
  gpioWrite(clk, PI_HIGH);
}

bool get_bit(uint16_t value, int n) {
  if (value & (1 << n)) {
    return 1;
  } else {
    return 0;
  }
}

void init_7seg_display() {

  gpioSetMode(data, PI_OUTPUT);  // make P0 output
  gpioSetMode(clk, PI_OUTPUT);   // make P0 output
  gpioSetMode(latch, PI_OUTPUT); // make P0 output

  gpioWrite(clk, PI_LOW);
  gpioWrite(latch, PI_LOW);
  // write_all(0);
}

void write_data_to_register(uint16_t value) {
  for (int i = 8 * chain - 1; i >= 0; --i) {
    push_bit(get_bit(value, i));
  }
}

uint8_t handle_dot(uint8_t value, bool turn_it_on) {
  return turn_it_on ? value & 0b01111111 : value;
}

const uint8_t available_chars[] = {
    // controls on/off of 7-segment led + dot. A bit is 0 means to turn that
    // segment led on.
    0b11000000, // 0
    0b11111001, // 1
    0b10100100, // 2
    0b10110000, // 3
    0b10011001, // 4
    0b10010010, // 5
    0b10000010, // 6
    0b11111000, // 7
    0b10000000, // 8
    0b10010000, // 9
    0b11111111, // empty
};

int show(uint8_t *values, bool *with_dots) {

  for (int i = 0; i < DIGIT_COUNT; ++i) {
    write_data_to_register(handle_dot(available_chars[values[i]], with_dots[i])
                               << 8 |
                           1 << (DIGIT_COUNT - 1 - i));
    // we pass a total of 16 bits to show():
    // 1st byte: controls on/off of 7-segment led + dot. A bit is 0 means to
    // turn that segment led on. 2nd byte: controls which digit the above
    // 7-segment definiton should be applied to.
    gpioWrite(latch, PI_HIGH);
    gpioWrite(latch, PI_LOW);
    usleep(20);
  }
  return 0;
}
