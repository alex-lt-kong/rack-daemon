#ifndef HTTP_SERVICE_APPLICATION_H
#define HTTP_SERVICE_APPLICATION_H

#include <cjson/cJSON.h>

// Caller takes the ownership of the return value and is respobsible free() it
cJSON *get_temp_control_json();
// Caller takes the ownership of the return value and is respobsible free() it
cJSON *get_rack_door_states_json();

#endif /* HTTP_SERVICE_APPLICATION_H */
