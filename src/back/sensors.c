#include "global_vars.h"

#include <cjson/cJSON.h>
#include <iotctrl/temp-sensor.h>

#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/syslog.h>
#include <unistd.h>

float get_reading_from_sensor(const char *path) {
  int temp_raw = iotctrl_get_temperature(path, 0);
  float temp_parsed = IOTCTRL_INVALID_TEMP;
  if (temp_raw == IOTCTRL_INVALID_TEMP) {
    syslog(LOG_ERR, "failed to read from sensor [%s]", path);
  } else {
    temp_parsed = temp_raw / 10.0;
  }
  return temp_parsed;
}

void load_sensor(const cJSON *json, const char *key, char **sensor_paths,
                 size_t *num_sensors) {
  cJSON *s;
  syslog(LOG_INFO, "Loading %s:", key);
  cJSON_ArrayForEach(s, cJSON_GetObjectItemCaseSensitive(json, key)) {
    if (cJSON_IsString(s)) {
      // sensor_paths[*num_sensors] will be pointing to s->valuestring, but
      // s->valuestring will only be free()ed at the very end of the program
      sensor_paths[*num_sensors] = s->valuestring;

      if (get_reading_from_sensor(sensor_paths[*num_sensors]) !=
          IOTCTRL_INVALID_TEMP) {
        syslog(LOG_INFO, "[%s] Loaded", sensor_paths[*num_sensors]);
        ++*num_sensors;
      } else {
        syslog(LOG_INFO, "[%s] malfunctions, will be excluded",
               sensor_paths[*num_sensors]);
      }
      if (*num_sensors >= MAX_SENSORS) {
        syslog(LOG_ERR,
               "Program supports up to %d %s only, extra "
               "sensors will not be used",
               MAX_SENSORS, key);
        break;
      }
    }
  }
  syslog(LOG_INFO, "%lu %s loaded", *num_sensors, key);
}

void load_sensors(const cJSON *json) {
  for (size_t i = 0; i < pl.num_int_sensors; ++i) {
    pl.int_temps[i] = BAD_TEMPERATURE;
  }
  for (size_t i = 0; i < pl.num_ext_sensors; ++i) {
    pl.ext_temps[i] = BAD_TEMPERATURE;
  }

  load_sensor(json, "external_sensors", pl.ext_sensor_paths,
              &pl.num_ext_sensors);
  load_sensor(json, "internal_sensors", pl.int_sensor_paths,
              &pl.num_int_sensors);
  syslog(LOG_INFO, "Loading internal sensors");
}

void save_temp_to_payload(char *sensors[], const size_t sensor_count,
                          float *temps, float *temp) {

  size_t valid_temps_count = 0;
  float _temp = 0;
  for (size_t i = 0; i < sensor_count && !ev_flag; ++i) {
    sleep(1);
    // takes around 1 sec to read value from one sensor.
    if ((temps[i] = get_reading_from_sensor(sensors[i])) !=
        IOTCTRL_INVALID_TEMP) {
      ++valid_temps_count;
      _temp += temps[i];
    }
    sleep(1);
  }
  if (valid_temps_count > 0) {
    _temp /= valid_temps_count;
  } else {
    _temp = 0;
  }
  // An intermediary variable canNOT be removed or there will be race condition
  *temp = _temp;
}
