/*
   pulse.c

   gcc -o pulse pulse.c -lpigpio -lrt -lpthread

   sudo ./pulse
*/

#include <stdio.h>
#include <unistd.h>
#include <pigpio.h>

#define   SDI   17   //serial data input
#define   RCLK  18   //memory clock input(STCP)
#define   SRCLK 11   //shift register clock input(SHCP)


unsigned char SegCode[17] = {0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f,0x77,0x7c,0x39,0x5e,0x79,0x71,0x80};

void init(void)
{
    gpioSetMode(SDI, 1); //make P0 output
    gpioSetMode(RCLK, 1); //make P0 output
    gpioSetMode(SRCLK, 1); //make P0 output

    gpioWrite(SDI, 0);
    gpioWrite(RCLK, 0);
    gpioWrite(SRCLK, 0);
}

void hc595_shift(unsigned char dat)
{
   printf("hc595_shift()\n");
    int i;

    for(i=0;i<8;i++){
        gpioWrite(SDI, 0b10000000 & (dat << i) > 0 ? 1 : 0);
        gpioWrite(SRCLK, 1);
        usleep(1000);
        gpioWrite(SRCLK, 0);
    }

        gpioWrite(RCLK, 1);
        usleep(1000);
        gpioWrite(RCLK, 0);
}

int test() {

    int i;

   if (gpioInitialise() < 0)
   {
      fprintf(stderr, "pigpio initialisation failed\n");
      return 1;
   }

    init();

    while(1){
        for(i=0;i<17;i++){
            hc595_shift(SegCode[i]);
            sleep(3);
        }
    }
   return 0;
}

int main(int argc, char *argv[])
{  
   test();
   return 0;
   double start;

   if (gpioInitialise() < 0)
   {
      fprintf(stderr, "pigpio initialisation failed\n");
      return 1;
   }

   /* Set GPIO modes */
   gpioSetMode(23, PI_OUTPUT);

   /* Start 1500 us servo pulses on GPIO4 */
  // gpioServo(4, 1500);

   /* Start 75% dutycycle PWM on GPIO17 */
   gpioPWM(23, 192); /* 192/255 = 75% */
   gpioSetPWMfrequency(23, 50); // Set GPIO23 to 50Hz.
   sleep(60);
   gpioPWM(23, 0); /* 192/255 = 75% */

   /* Stop DMA, release resources */
   gpioTerminate();

   return 0;
}