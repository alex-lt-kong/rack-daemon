#ifndef HTTP_SERVICE_APPLICATION_H
#define HTTP_SERVICE_APPLICATION_H

#include <cjson/cJSON.h>

cJSON *get_temp_control_json();
void get_rack_door_states_json();

#endif /* HTTP_SERVICE_APPLICATION_H */
