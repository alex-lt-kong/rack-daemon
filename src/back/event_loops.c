#include "event_loops.h"
#include "7seg_display.h"
#include "database.h"
#include "global_vars.h"
#include "sensors.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <sys/syslog.h>
#include <unistd.h>

void *ev_set_7seg_display() {
  syslog(LOG_INFO, "ev_set_7seg_display() started.");
  init_7seg_display();
  uint8_t values[DIGIT_COUNT];
  bool dots[DIGIT_COUNT] = {0, 0, 1, 0, 0, 0, 1, 0};

  while (!ev_flag) {
    // We need to use an intermediary variable to avoid accessing pl members
    // multiple times; otherwise we can still trigger race condition

    const uint16_t fl = pl.fans_load * 10;
    const int _int_temp = pl.int_temp;

    values[0] = 10; // means turning the digit off
    values[1] = _int_temp % 100000 / 10000;
    values[2] = _int_temp % 10000 / 1000;
    values[3] = _int_temp % 1000 / 100;

    values[4] = fl % 10000 / 1000;
    if (values[4] == 0) {
      values[4] = 10; // means turning the digit off
    }
    values[5] = fl % 1000 / 100;
    values[6] = fl % 100 / 10;
    values[7] = fl % 10;
    show(values, dots);
  }
  char exit_msg[] = "ev_set_7seg_display() quits gracefully.\n";
  syslog(LOG_INFO, "%s", exit_msg);
  return NULL;
}

void *ev_get_temp_from_sensors() {
  syslog(LOG_INFO, "ev_get_temp_from_sensors() started.");

  while (!ev_flag) {
    save_temp_to_payload(pl.int_sensor_paths, pl.num_int_sensors, pl.int_temps,
                         &pl.int_temp);
    save_temp_to_payload(pl.ext_sensor_paths, pl.num_ext_sensors, pl.ext_temps,
                         &pl.ext_temp);
  }
  syslog(LOG_INFO, "ev_get_temp_from_sensors() quits gracefully.");
  return NULL;
}

void *ev_apply_fans_load() {

  syslog(LOG_INFO, "ev_apply_fans_load() started.");

  const uint16_t fans_pin = 23;
  const size_t interval_sec = 1800;
  gpioSetMode(fans_pin, PI_OUTPUT);
  gpioSetPWMfrequency(fans_pin, 50); // Set GPIO23 to 50Hz.

  // Wait for all sensors to be read at least once
  sleep((pl.num_ext_sensors + pl.num_int_sensors) * 5);

  while (!ev_flag) {
    // Strictly speaking, there could be race conditions, but the consequence
    // should be fine for this particular purpose
    int _fans_load = (pl.int_temp == pl.ext_temp && pl.int_temp == 0)
                         ? 0
                         : ((pl.int_temp - pl.ext_temp) / 10 / 6);
    // i.e., (int_temp - ext_temp) > 6 degrees Celsius means 100% fans load
    _fans_load = _fans_load > 100 ? 100 : _fans_load;
    _fans_load = _fans_load < 0 ? 0 : _fans_load;
    pl.fans_load = _fans_load;

    if (gpioPWM(fans_pin, pl.fans_load / 100.0 * 254) != 0) {
      syslog(LOG_ERR, "Failed to set new fans load.");
    }
    save_data_to_db();
    for (size_t i = 0; i < interval_sec && !ev_flag; ++i) {
      sleep(1);
    }
  }
  syslog(LOG_INFO, "ev_apply_fans_load() quits gracefully.");
  return NULL;
}

void *ev_monitor_rack_door() {
  syslog(LOG_INFO, "ev_monitor_rack_door() started.");
  const int pin_pos = 19;
  const int pin_neg = 16;

  gpioSetMode(pin_pos, PI_OUTPUT);
  gpioSetMode(pin_neg, PI_INPUT);
  gpioWrite(pin_pos, PI_HIGH);
  gpioSetPullUpDown(pin_neg, PI_PUD_UP);

  // false -> circuit opened -> door opened
  // true -> circuit closed -> door closed
  bool last_status = true;
  bool current_status = true;
  size_t status_count = 0;
  while (!ev_flag) {

    usleep(1000 * 1000); // i.e., 1sec
    current_status = gpioRead(pin_neg);
    if (current_status != last_status) {
      ++status_count;
      if (status_count < 5) {
        continue;
      }
      last_status = current_status;
      save_rack_door_state_to_db(current_status);
    } else {
      status_count = 0;
    }
  }
  syslog(LOG_INFO, "ev_monitor_rack_door() quits gracefully.");
  return NULL;
}
