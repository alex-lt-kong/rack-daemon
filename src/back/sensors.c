#include "global_vars.h"

#include <cjson/cJSON.h>

#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/syslog.h>
#include <unistd.h>

void load_sensors(const cJSON *json) {
  for (size_t i = 0; i < pl.num_int_sensors; ++i) {
    pl.int_temps[i] = BAD_TEMPERATURE;
  }
  for (size_t i = 0; i < pl.num_ext_sensors; ++i) {
    pl.ext_temps[i] = BAD_TEMPERATURE;
  }
  cJSON *s;
  syslog(LOG_INFO, "Loading external sensors:");
  cJSON_ArrayForEach(
      s, cJSON_GetObjectItemCaseSensitive(json, "external_sensors")) {
    if (cJSON_IsString(s)) {
      pl.ext_sensor_paths[pl.num_ext_sensors] = strdup(s->valuestring);
      syslog(LOG_INFO, "%s", pl.ext_sensor_paths[pl.num_ext_sensors]);
      ++pl.num_ext_sensors;
      if (pl.num_ext_sensors >= MAX_SENSORS) {
        fprintf(stderr,
                "Program supports up to %d external sensors only, extra "
                "sensors will not be used",
                MAX_SENSORS);
        break;
      }
    }
  }
  syslog(LOG_INFO, "%lu external sensors loaded", pl.num_ext_sensors);

  syslog(LOG_INFO, "Loading internal sensors");
  cJSON_ArrayForEach(
      s, cJSON_GetObjectItemCaseSensitive(json, "internal_sensors")) {
    if (cJSON_IsString(s)) {
      pl.int_sensor_paths[pl.num_int_sensors] = strdup(s->valuestring);
      syslog(LOG_INFO, "%s", pl.int_sensor_paths[pl.num_int_sensors]);
      ++pl.num_int_sensors;
      if (pl.num_int_sensors >= MAX_SENSORS) {
        fprintf(stderr,
                "Program supports up to %d external sensors only, extra "
                "sensors will not be used",
                MAX_SENSORS);
        break;
      }
    }
  }
  syslog(LOG_INFO, "%lu internal sensors loaded", pl.num_int_sensors);
}

void save_temp_to_payload(char *sensors[], const size_t sensor_count,
                          int32_t *temps, int32_t *temp) {
  char buf[PATH_MAX];
  int fd;
  size_t valid_temps_count = 0;
  int _temp = 0;
  for (size_t i = 0; i < sensor_count && !ev_flag; ++i) {
    sleep(1);
    // takes around 1 sec to read value from one sensor.
    fd = open(sensors[i], O_RDONLY);
    if (fd >= 0) {
      if (read(fd, buf, sizeof(buf)) > 0) {
        char *temp_str = strstr(buf, "t=") + 2;
        sscanf(temp_str, "%d", &(temps[i]));
      }
      close(fd);
      _temp += temps[i];
      ++valid_temps_count;
    } else {
      temps[i] = BAD_TEMPERATURE;
      syslog(LOG_ERR,
             "Unable to open device at [%s], errno: %d(%s), skipped this "
             "read attempt.",
             sensors[i], errno, strerror(errno));
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
