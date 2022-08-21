#include <stdio.h>
#include <unistd.h>
#include <pigpio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <sqlite3.h>
#include <syslog.h>

#include "7seg.c"

struct Payload {
  int32_t temps[4];
  float fans_load;
};

volatile sig_atomic_t done = 0;

void signal_handler(int signum) {
  syslog(LOG_INFO, "Signal %d received by signal_handler()\n", signum);
  done = 1;
}

void* thread_monitor_rack_door() {
   syslog(LOG_INFO, "thread_monitor_rack_door() started.");
   const int pin_pos = 19;
   const int pin_neg = 16;

   gpioSetMode(pin_pos, PI_OUTPUT);
   gpioSetMode(pin_neg, PI_INPUT);
   gpioWrite(pin_pos, PI_HIGH);
   gpioSetPullUpDown(pin_neg, PI_PUD_UP);

   bool last_status = false;
   bool current_status = false;
   while (!done) {
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
   syslog(LOG_INFO, "thread_monitor_rack_door() quits gracefully.");
}

void* thread_apply_fans_load(void* payload) {
   
   syslog(LOG_INFO, "thread_apply_fans_load() started.");
   struct Payload* pl = (struct Payload*)payload;
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
   char *sqlite_err_msg = 0;
   time_t now;

   const uint16_t fans_pin = 23;
   double fans_load = 0.5;;
   gpioSetMode(fans_pin, PI_OUTPUT);
   gpioSetPWMfrequency(fans_pin, 50); // Set GPIO23 to 50Hz.
   
   uint32_t iter = 0;
   sqlite3 *db;
   while (!done) {
      sleep(1);
      iter += 1;
      //printf("temps: %d, %d, %d, %d\n", pl->temps[0], pl->temps[1], pl->temps[2], pl->temps[3]);
      int16_t delta = ((pl->temps[0] + pl->temps[1]) - (pl->temps[2] + pl->temps[3])) / 2;
      fans_load = delta / 1000.0 / 10.0;
      //printf("delta: %d, fans_load_raw: %f, ", delta, fans_load);
      fans_load = fans_load > 1.0 ? 1.0 : fans_load;
      fans_load = fans_load < 0.0 ? 0 : fans_load;
      //printf("fans_load_regulated: %f\n", fans_load);
      if (gpioPWM(fans_pin, fans_load * 254) != 0) {
         syslog(LOG_ERR, "Failed to set new fans load.");
         continue;
      }      
      if (iter < 600) {
         continue;
      }
      iter = 0;
      
      
      int rc = sqlite3_open("data.sqlite", &db);      
      if (rc != SQLITE_OK) {         
         syslog(LOG_ERR, "Cannot open database: %s. INSERT will be skipped\n", sqlite3_errmsg(db));
         sqlite3_close(db);
         continue;
      }
      rc = sqlite3_exec(db, sql_create, 0, 0, &sqlite_err_msg);      
      if (rc != SQLITE_OK) {         
         syslog(LOG_ERR, "SQL error: %s. INSERT is not successful.\n", sqlite_err_msg);         
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
         sqlite3_bind_text(stmt, 1, buf, -1, NULL);
         sqlite3_bind_int(stmt, 2, pl->temps[0]);
         sqlite3_bind_int(stmt, 3, pl->temps[1]);
         sqlite3_bind_int(stmt, 4, pl->temps[2]);
         sqlite3_bind_int(stmt, 5, pl->temps[3]);;
         sqlite3_bind_double(stmt, 6, fans_load);
         sqlite3_step(stmt);
         rc = sqlite3_finalize(stmt);
         if (rc != SQLITE_OK) {         
            syslog(LOG_ERR, "SQL error: %d. INSERT is not successful.\n", rc);         
            sqlite3_free(sqlite_err_msg);        
            sqlite3_close(db);
            continue;
         }
      }
      sqlite3_close(db);
   }
   syslog(LOG_INFO, "thread_apply_fans_load() quits gracefully.");
}

void* thread_get_readings_from_sensors(void* payload) {
   syslog(LOG_INFO, "thread_get_readings_from_sensors() started.");
   struct Payload* pl = (struct Payload*)payload;
   
   char sensors[4][256] = {
      "/sys/bus/w1/devices/28-0301a279faf2/w1_slave",
      "/sys/bus/w1/devices/28-030997792b61/w1_slave",
      "/sys/bus/w1/devices/28-01144ebe52aa/w1_slave",
      "/sys/bus/w1/devices/28-01144ef1faaa/w1_slave"
   };
   char buf[256];
   int fd;

   while (!done) {
      for (int i = 0; i < 4 && !done; ++i) {
         // takes around 1 sec to read value from one sensor.
         fd = open(sensors[i], O_RDONLY);
         if(fd >= 0) {
            if(read( fd, buf, sizeof(buf) ) > 0) {          
               char* temp_str = strstr(buf, "t=") + 2;
               sscanf(temp_str, "%d", &(pl->temps[i]));
            }
            close(fd);
         } else {
            syslog(LOG_ERR, "Unable to open device at [%s], skipped this read attempt.", sensors[i]);
         }
         sleep(1);
      }
   }
   syslog(LOG_INFO, "thread_get_readings_from_sensors() quits gracefully.");
}

void* thread_set_7seg_display(void* payload) {
   syslog(LOG_INFO, "thread_set_7seg_display() started.");
   struct Payload* pl = (struct Payload*)payload;
   init_7seg_display();
   uint8_t values[DIGIT_COUNT];
   bool dots[DIGIT_COUNT] = {0,0,1,0,0,0,1,0};
   int32_t internal_temp;

   while (!done) {
      internal_temp = (pl->temps[0] + pl->temps[1]) / 2;
      values[0] = 10;
      values[1] = internal_temp % 100000 / 10000;
      values[2] = internal_temp % 10000 / 1000;
      values[3] = internal_temp % 1000 / 100;

      uint16_t fl = pl->fans_load * 1000;
      values[4] = 10;
      values[5] = fl % 10000 / 1000;
      values[6] = fl % 1000 / 100;
      values[7] = fl % 100 / 10;
      
      show(values, dots);
   }
   
   syslog(LOG_INFO, "thread_set_7seg_display() quits gracefully.");
}

int main(int argc, char *argv[])
{
   openlog("rm.out", LOG_PID | LOG_CONS, 0);
   syslog(LOG_INFO, "rm.out started\n", argv[0]);
   pthread_t tids[4];

   if (gpioInitialise() < 0) {
      syslog(LOG_ERR, "pigpio initialisation failed, program will quit\n");
      closelog();
      return 1;
   }
   struct Payload pl;
   pl.temps[0] = 65535;
   pl.temps[1] = 65535;
   pl.temps[2] = 65535;
   pl.temps[3] = 65535;
   if (
      pthread_create(&tids[0], NULL, thread_get_readings_from_sensors, &pl) != 0 ||
      pthread_create(&tids[1], NULL, thread_apply_fans_load, &pl) != 0 ||
      pthread_create(&tids[2], NULL, thread_monitor_rack_door, NULL) != 0 ||
      pthread_create(&tids[3], NULL, thread_set_7seg_display, &pl) != 0
   ) {
      syslog(LOG_ERR, "Failed to create essential threads, program will quit\n");
      done = 1;
      closelog();
      return 1;
   }

   struct sigaction act;
   act.sa_handler = signal_handler;
   sigemptyset(&act.sa_mask);
   act.sa_flags = SA_RESETHAND;
   sigaction(SIGINT, &act, 0);
   sigaction(SIGABRT, &act, 0);
   sigaction(SIGTERM, &act, 0);
   for (int i = 0; i < sizeof(tids) / sizeof(tids[0]); ++i) {
      pthread_join(tids[i], NULL);
   }
   /* Stop DMA, release resources */
   gpioTerminate();
   syslog(LOG_INFO, "Program quits gracefully.");
   closelog();
   return 0;
}