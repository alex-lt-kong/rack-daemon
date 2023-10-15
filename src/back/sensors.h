#include "global_vars.h"

#include <cjson/cJSON.h>
#ifndef SENSORS_H
#define SENSORS_H

void load_sensors(const cJSON *json);

void save_temp_to_payload(char *sensors[], const size_t sensor_count,
                          int32_t *temps, int32_t *temp);

#endif /* SENSORS_H */
