#include <stdio.h>
#include <unistd.h>
#include <pigpio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include "7seg.c"

void* thread_change_fans_load(void* fans_load) {
   while (1) {
      gpioPWM(23, *((float*)fans_load) * 254); /* 192/255 = 75% */
      sleep(1);
   }
}

void* thread_calculate_fans_load(void* fans_load) {
   char sensors[4][256] = {
      "/sys/bus/w1/devices/28-0301a279faf2/w1_slave",
      "/sys/bus/w1/devices/28-030997792b61/w1_slave",
      "/sys/bus/w1/devices/28-01144ebe52aa/w1_slave",
      "/sys/bus/w1/devices/28-01144ef1faaa/w1_slave"
   };
   char buf[256];
   int16_t temps[] = {0, 0, 0, 0};
   int fd;
   while (1) {
      for (int i = 0; i < 4; ++i) {
         fd = open(sensors[i], O_RDONLY);
         if(fd >= 0) {
            if(read( fd, buf, sizeof(buf) ) > 0) {          
               char* temp_str = strstr(buf, "t=") + 2;
               sscanf(temp_str, "%d", &temps[i]);
            }
            close(fd);
         } else {
            fprintf(stderr, "Unable to open device at [%s], skipped this read iteration.\n", sensors[i]);
         }
      }
      int16_t delta = (temps[0] + temps[1]) - (temps[2] + temps[3]);
      *((float*)fans_load) = delta / 1000.0 / 10.0;
      printf("%f\n", *((float*)fans_load));
      sleep(1);
   }
}

int main(int argc, char *argv[])
{
   double start;

   if (gpioInitialise() < 0)
   {
      fprintf(stderr, "pigpio initialisation failed\n");
      return 1;
   }
   float fans_load = 0;
   pthread_t id;
   if (pthread_create(&id, NULL, thread_calculate_fans_load, &fans_load) != 0) {
      fprintf(stderr, "Failed to create calculate_load() thread, program will quit");
      return 1;
   }
   gpioSetMode(23, PI_OUTPUT);
   gpioSetPWMfrequency(23, 50); // Set GPIO23 to 50Hz.
   pthread_t id1;   
   if (pthread_create(&id1, NULL, thread_change_fans_load, &fans_load) != 0) {
      fprintf(stderr, "Failed to create thread_change_fans_load() thread, program will quit");
      return 1;
   }
   /* Set GPIO modes */
   
   
   init_7seg_display();
   uint8_t values[4];
   bool dots[] = {false, false, true, false};

   while (1) {
      uint16_t fl = fans_load * 1000;
      values[0] = fl / 1000;
      values[1] = fl % 1000 / 100;
      values[2] = fl % 100 / 10;
      values[3] = fl % 10;
      show(values, dots);
   }
   /* Stop DMA, release resources */
   gpioTerminate();

   return 0;
}