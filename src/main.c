#include <stdio.h>
#include <unistd.h>
#include <pigpio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <sqlite3.h>
#include "7seg.c"

float fans_load = 0;

void* thread_monitor_rack_door() {

   int pin_pos = 19;
   int pin_neg = 16;

   gpioSetMode(pin_pos, PI_OUTPUT);
   gpioSetMode(pin_neg, PI_INPUT);
   gpioWrite(pin_pos, PI_HIGH);
   gpioSetPullUpDown(pin_neg, PI_PUD_UP);

   bool last_status = false;
   bool current_status = false;
   while (1) {
      current_status = gpioRead(pin_neg);
      if (current_status != last_status) {
         if (current_status == true) {
            printf("Door is closed\n");
         } else {
            printf("Door is opened\n");
         }
         last_status = current_status;
      }
      usleep(500 * 1000);
   }
}

void* thread_change_fans_load(void* fans_load) {
   uint16_t fans_pin = 23;
   gpioSetMode(fans_pin, PI_OUTPUT);
   gpioSetPWMfrequency(23, 50); // Set GPIO23 to 50Hz.
   while (1) {
      gpioPWM(fans_pin, *((float*)fans_load) * 254); /* 192/255 = 75% */
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
   int32_t temps[] = {0, 0, 0, 0};
   int fd;
   uint32_t iter = 0;
   sqlite3 *db;
   char *sqlite_err_msg = 0;
   const char* sql_create = "CREATE TABLE IF NOT EXISTS temp_control"
               "("
               "  [record_id] INTEGER PRIMARY KEY AUTOINCREMENT,"
               "  [record_time] TEXT,"
               "  [external_temp_0] INTEGER,"
               "  [external_temp_1] INTEGER,"
               "  [internal_temp_0] INTEGER,"
               "  [internal_temp_1] INTEGER,"
               "  [fans_load] REAL"
               ")";
   const char* sql_insert = "INSERT INTO temp_control"
            "(record_time, external_temp_0, external_temp_1, internal_temp_0, internal_temp_1, fans_load) "
            "VALUES(?, ?, ?, ?, ?, ?);";
   time_t now;

   while (1) {
      iter += 1;

      for (int i = 0; i < 4; ++i) {
         // takes around 1 sec to read value from one sensor.
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
      //printf("%f\n", *((float*)fans_load));
      sleep(1);
      if (iter % 2 != 0) {
         continue;
      }
      
      int rc = sqlite3_open("data.sqlite", &db);      
      if (rc != SQLITE_OK) {         
         fprintf(stderr, "Cannot open database: %s. INSERT will be skipped\n", sqlite3_errmsg(db));
         sqlite3_close(db);
         continue;
      }
      rc = sqlite3_exec(db, sql_create, 0, 0, &sqlite_err_msg);      
      if (rc != SQLITE_OK) {         
         fprintf(stderr, "SQL error: %s. INSERT is not successful.\n", sqlite_err_msg);         
         sqlite3_free(sqlite_err_msg);        
         sqlite3_close(db);
         continue;
      }
      sqlite3_stmt *stmt;
      sqlite3_prepare_v2(db, sql_insert, 512, &stmt, NULL);
      if(stmt != NULL) {
         
         time(&now);
         char buf[sizeof("1970-01-01T00:00:00Z")];
         strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", gmtime(&now)); 
         sqlite3_bind_text(stmt, 1, buf, SQLITE_TRANSIENT, NULL);
         sqlite3_bind_int(stmt, 2, temps[0]);
         sqlite3_bind_int(stmt, 3, temps[1]);
         sqlite3_bind_int(stmt, 4, temps[2]);
         sqlite3_bind_int(stmt, 5, temps[3]);;
         sqlite3_bind_double(stmt, 6, *((float*)fans_load));
         sqlite3_step(stmt);
         rc = sqlite3_finalize(stmt);
         if (rc != SQLITE_OK) {         
            fprintf(stderr, "SQL error: %d. INSERT is not successful.\n", rc);         
            sqlite3_free(sqlite_err_msg);        
            sqlite3_close(db);
            continue;
         }
      }
      sqlite3_close(db);
   }
}

int main(int argc, char *argv[])
{
   double start;
   pthread_t tids[4];

   if (gpioInitialise() < 0) {
      fprintf(stderr, "pigpio initialisation failed\n");
      return 1;
   }
   
   if (pthread_create(&tids[0], NULL, thread_calculate_fans_load, &fans_load) != 0) {
      fprintf(stderr, "Failed to create calculate_load() thread, program will quit");
      return 1;
   }   
   if (pthread_create(&tids[1], NULL, thread_change_fans_load, &fans_load) != 0) {
      fprintf(stderr, "Failed to create thread_change_fans_load() thread, program will quit");
      return 1;
   }
   if (pthread_create(&tids[2], NULL, thread_monitor_rack_door, NULL) != 0) {
      fprintf(stderr, "Failed to create thread_monitor_rack_door() thread, program will quit");
      return 1;
   }
   
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