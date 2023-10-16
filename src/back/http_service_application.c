#include "database.h"
#include "global_vars.h"
#include "utils.h"

#include <cjson/cJSON.h>

#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <time.h>

cJSON *get_temp_control_json() {
  cJSON *dto = cJSON_CreateObject();
  if (dto == NULL) {
    syslog(LOG_ERR, "cJSON_CreateObject() returns NULL");
    return NULL;
  }
  const size_t external_temps_size = pl.num_ext_sensors * 6 + 1;
  const size_t internal_temps_size = pl.num_int_sensors * 6 + 1;
  char external_temps_str[external_temps_size];
  char internal_temps_str[internal_temps_size];
  (void)concat_int_arr_to_cstr(pl.num_ext_sensors, pl.ext_temps,
                               external_temps_str);
  (void)concat_int_arr_to_cstr(pl.num_int_sensors, pl.int_temps,
                               internal_temps_str);
  cJSON_AddItemToObject(dto, "external_temps",
                        cJSON_CreateString(external_temps_str));
  cJSON_AddItemToObject(dto, "internal_temps",
                        cJSON_CreateString(internal_temps_str));
  time_t now;
  time(&now);
  char dt_buffer[sizeof(SAMPLE_ISO_DT_STRING)];
  strftime(dt_buffer, sizeof dt_buffer, "%Y-%m-%d %H:%M:%S", localtime(&now));
  cJSON_AddItemToObject(dto, "record_time", cJSON_CreateString(dt_buffer));
  cJSON_AddItemToObject(dto, "fans_load", cJSON_CreateNumber(pl.fans_load));
  return dto;
}

void get_rack_door_states_json() {
  const size_t max_row_count = 6;
  int ids[max_row_count];
  char record_times[max_row_count][sizeof(SAMPLE_ISO_DT_STRING)];
  int states[max_row_count];
  const ssize_t row_count = get_top_six_door_states(ids, record_times, states);
  for (ssize_t i = 0; i < row_count; ++i) {
    printf("%d\t%s\t%d\n", ids[i], record_times[i], states[i]);
  }
  printf("\n");
}
