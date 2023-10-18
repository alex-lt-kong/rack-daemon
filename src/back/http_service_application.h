#ifndef HTTP_SERVICE_APPLICATION_H
#define HTTP_SERVICE_APPLICATION_H

#include <cjson/cJSON.h>

// Caller takes the ownership of the return value and is respobsible free() it
cJSON *get_temp_control_json();
// Caller takes the ownership of the return value and is respobsible free() it
cJSON *get_rack_door_states_json();
// Caller takes the ownership of the return value and is respobsible free() it
char *read_file(const char *image_directory, const char *filename,
                size_t *length);
// Caller takes the ownership of the return value and is respobsible free() it
cJSON *get_images_list_json(const char *image_directory);
#endif /* HTTP_SERVICE_APPLICATION_H */
