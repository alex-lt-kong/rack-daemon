#ifndef GLOBAL_VARS_H
#define GLOBAL_VARS_H

#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#define MAX_SENSORS 16

struct Payload {
  /* The program relies on glibc's implicit guarantee to achieve "lock-free"
   * design goal:
   * https://www.gnu.org/software/libc/manual/html_node/Atomic-Types.html.
   * As a result, all members of Payload have to be either char or int;
   * otherwise, we may need to use mutex to aovid data corruption*/
  int32_t int_temps[MAX_SENSORS];
  int32_t ext_temps[MAX_SENSORS];
  int32_t int_temp;
  int32_t ext_temp;
  char *int_sensor_paths[MAX_SENSORS];
  char *ext_sensor_paths[MAX_SENSORS];
  size_t num_ext_sensors;
  size_t num_int_sensors;
  int32_t fans_load;
};

extern struct Payload pl;
extern volatile sig_atomic_t ev_flag;

#endif /* GLOBAL_VARS_H */
