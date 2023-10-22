#ifndef GLOBAL_VARS_H
#define GLOBAL_VARS_H

#include <signal.h>
#include <stddef.h>
#include <stdint.h>

#define BAD_TEMPERATURE 65535
#define MAX_SENSORS 16
#define SAMPLE_ISO_DT_STRING "1970-01-01 00:00:00"

struct Payload {
  /* The program relies on glibc's implicit guarantee to achieve "lock-free"
   * design goal:
   * https://www.gnu.org/software/libc/manual/html_node/Atomic-Types.html.
   * As a result, all members of Payload have to be either char or int;
   * otherwise, we may need to use mutex to aovid data corruption*/
  float int_temps[MAX_SENSORS];
  float ext_temps[MAX_SENSORS];
  float int_temp;
  float ext_temp;
  char *int_sensor_paths[MAX_SENSORS];
  char *ext_sensor_paths[MAX_SENSORS];
  size_t num_ext_sensors;
  size_t num_int_sensors;
  int32_t fans_load;
};

extern struct Payload pl;
extern volatile sig_atomic_t ev_flag;

#endif /* GLOBAL_VARS_H */
