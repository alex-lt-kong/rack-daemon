#ifndef RD_UTILS_H
#define RD_UTILS_H

#include <stdint.h>
#include <stdio.h>

int concat_float_arr_to_cstr(const size_t arr_size, const float *arr,
                             const size_t dest_size, char *dest_str);

#endif /* UTILS_H */
