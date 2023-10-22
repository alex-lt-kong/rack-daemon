#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>

int concat_float_arr_to_cstr(const size_t arr_size, const float *arr,
                             const size_t dest_size, char *dest_str) {
  int offset = 0;
  for (size_t i = 0; i < arr_size; ++i) {
    int written = snprintf(dest_str + offset, dest_size-offset, "%f,", arr[i]);
    if (written < 0) {
      syslog(LOG_ERR,
             "Error converting integer to string, current c-string is: %s",
             dest_str);
      return -1;
    }
    offset += written;
  }
  if (strnlen(dest_str, dest_size) > 0) {
    dest_str[strnlen(dest_str, dest_size) - 1] = '\0';
  }
  return 0;
}
