// This is a brute-force translation of https://github.com/shrikantpatnaik/Pi7SegPy/blob/master/Pi7SegPy.py

#include <pigpio.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>


int data = 17;
int clk = 11;
int latch = 18;
int chain = 2;


void push_bit(int bit) {
    gpioWrite(clk, PI_LOW);
    gpioWrite(data, bit);
    gpioWrite(clk, PI_HIGH);
}

int get_bit(unsigned int value, int n) {
    if (value & (1 << n)) {
        return 1;
    } else {
        return 0 ;
    }
}
/*
void write_all(int val) {
  for (int i = 0; i < 8 * chain; ++i) {
    push_bit(val);
  }
  write_latch();
}*/

void initt() {
  /*
      GPIO.setwarnings(False)
    GPIO.setmode(GPIO.BCM)
    GPIO.setup(data, GPIO.OUT)
    GPIO.setup(clock, GPIO.OUT, initial=GPIO.LOW)
    GPIO.setup(latch, GPIO.OUT, initial=GPIO.LOW)
    write_all(0)
    */
    gpioSetMode(data, PI_OUTPUT); //make P0 output
    gpioSetMode(clk, PI_OUTPUT); //make P0 output
    gpioSetMode(latch, PI_OUTPUT); //make P0 output

    gpioWrite(clk, PI_LOW);
    gpioWrite(latch, PI_LOW);
   // write_all(0);
}


int show(unsigned int value) {
    if (value > 0b1111111111111111){ // value.bit_length() > (8*chain):{
       fprintf(stderr, "Tried to write more bits than available\n");
    }
    for (int i = 8 * chain - 1; i >= 0; --i) {
      push_bit(get_bit(value, i));
    }
    
}

unsigned int append_dot(unsigned int value, bool with) {
    return with ? value & ~(1 << 7) : value;
}

unsigned int available_chars[] = {
  0b11000000,
  0b11111001,
  0b10100100,
  0b10110000,
  0b10011001,
  0b10010010,
  0b10000011,
  0b11111000,
  0b10000000,
  0b10011000,
};

int main() {
  if (gpioInitialise() < 0)
   {
      fprintf(stderr, "pigpio initialisation failed\n");
      return 1;
   }
   initt();
   int vals[] = {3,1,4,2};
   int with_dots[] = {1,0,0,0};
  while (1){
    
    for (int i = 0; i < 4; ++i) {
      show(append_dot(available_chars[vals[i]], with_dots[i]) << 8 | 1 << (3-i));      
      gpioWrite(latch, PI_HIGH);
      gpioWrite(latch, PI_LOW); 
      usleep(5000);
    }
    
  }

  return 0;
}
