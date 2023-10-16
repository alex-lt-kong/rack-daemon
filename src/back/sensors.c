#include "global_vars.h"

#include <cjson/cJSON.h>

#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/syslog.h>
#include <unistd.h>

int get_reading_from_sensor(const char *path) {
  char buf[PATH_MAX];
  int fd;
  int temp;
  fd = open(path, O_RDONLY);
  if (fd >= 0) {
    if (read(fd, buf, sizeof(buf)) > 0) {
      char *temp_str = strstr(buf, "t=") + 2;
      (void)sscanf(temp_str, "%d", &temp);
    } else {
      temp = BAD_TEMPERATURE;
      syslog(LOG_ERR, "read() from %s failed: %d(%s)", path, errno,
             strerror(errno));
    }
    (void)close(fd);
  } else {
    temp = BAD_TEMPERATURE;
    syslog(LOG_ERR,
           "Unable to open device at [%s], errno: %d(%s), skipped this "
           "read attempt.",
           path, errno, strerror(errno));
  }
  return temp;
}

void load_sensor(const cJSON *json, const char *key, char **sensor_paths,
                 size_t *num_sensors) {
  cJSON *s;
  syslog(LOG_INFO, "Loading %s:", key);
  cJSON_ArrayForEach(s, cJSON_GetObjectItemCaseSensitive(json, key)) {
    if (cJSON_IsString(s)) {
      sensor_paths[*num_sensors] = strdup(s->valuestring);
      syslog(LOG_INFO, "%s", sensor_paths[*num_sensors]);
      if (get_reading_from_sensor(sensor_paths[*num_sensors]) !=
          BAD_TEMPERATURE) {
        ++*num_sensors;
      } else {
        syslog(LOG_INFO, "Malfunctioning %s [%s] will be excluded", key,
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
                          int32_t *temps, int32_t *temp) {

  size_t valid_temps_count = 0;
  int _temp = 0;
  for (size_t i = 0; i < sensor_count && !ev_flag; ++i) {
    sleep(1);
    // takes around 1 sec to read value from one sensor.
    if ((temps[i] = get_reading_from_sensor(sensors[i])) != BAD_TEMPERATURE) {
      ++valid_temps_count;
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
