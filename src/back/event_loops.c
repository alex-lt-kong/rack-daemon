#include "event_loops.h"
#include "database.h"
#include "global_vars.h"
#include "sensors.h"

#include <iotctrl/7segment-display.h>

#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <pigpio.h>
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <sys/syslog.h>
#include <unistd.h>

void *ev_update_temps() {
  syslog(LOG_INFO, "ev_update_temps() started.");
  // TODO: move to JSON config
  const struct iotctrl_7seg_display_connection_info conn = {8, 17, 11, 18, 2};
  int ret_7seg;
  if ((ret_7seg = iotctrl_init_display("/dev/gpiochip0", conn)) != 0) {
    syslog(LOG_ERR,
           "iotctrl_init_display() failed: %d. 7seg display will be disabled",
           ret_7seg);
  }
  while (!ev_flag) {
    write_temps_payload(pl.int_sensor_paths, pl.num_int_sensors, pl.int_temps,
                        &pl.int_temp);
    write_temps_payload(pl.ext_sensor_paths, pl.num_ext_sensors, pl.ext_temps,
                        &pl.ext_temp);
    if (ret_7seg != 0) {
      continue;
    }
    iotctrl_update_value_two_four_digit_floats(pl.int_temp, pl.fans_load);
  }
  iotctrl_finalize_7seg_display();
  syslog(LOG_INFO, "ev_update_temps() quits gracefully.");
  return NULL;
}

void *ev_update_fans_load() {

  syslog(LOG_INFO, "ev_update_fans_load() started.");

  const uint16_t fans_pin = 23;
  const size_t interval_sec = 1800;
  gpioSetMode(fans_pin, PI_OUTPUT);
  gpioSetPWMfrequency(fans_pin, 50); // Set GPIO23 to 50Hz.

  // Wait for all sensors to be read at least once
  sleep((pl.num_ext_sensors + pl.num_int_sensors + 1) * 5);

  while (!ev_flag) {
    // Strictly speaking, there could be race conditions, but the consequence
    // should be fine for this particular purpose
    int _fans_load = (pl.int_temp == pl.ext_temp && pl.int_temp == 0)
                         ? 0
                         : ((pl.int_temp - pl.ext_temp) / 8.0 * 100);
    // i.e., (int_temp - ext_temp) > 8 degrees Celsius means 100% fans load
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
  syslog(LOG_INFO, "ev_update_fans_load() quits gracefully.");
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
