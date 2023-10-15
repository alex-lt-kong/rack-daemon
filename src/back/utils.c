#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>

int concat_int_arr_to_cstr(const size_t arr_size, const int32_t *arr,
                           char *dest_str) {
  int offset = 0;
  for (size_t i = 0; i < arr_size; ++i) {
    int written = sprintf(dest_str + offset, "%d,", arr[i]);
    if (written < 0) {
      syslog(LOG_ERR,
             "Error converting integer to string, current c-string is: %s",
             dest_str);
      return -1;
    }
    offset += written;
  }
  if (strlen(dest_str) > 0) {
    dest_str[strlen(dest_str) - 1] = '\0';
  }
  return 0;
}
