#include "global_vars.h"

#include <cjson/cJSON.h>
#ifndef RD_SENSORS_H
#define RD_SENSORS_H

void load_sensors(const cJSON *json);

void save_temp_to_payload(char *sensors[], const size_t sensor_count,
                          float *temps, float *temp);

#endif /* SENSORS_H */
