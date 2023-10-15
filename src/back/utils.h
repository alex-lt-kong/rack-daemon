#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stdio.h>

int concat_int_arr_to_cstr(const size_t arr_size, const int32_t *arr,
                           char *dest_str);

#endif /* UTILS_H */
