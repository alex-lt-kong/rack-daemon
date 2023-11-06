#include "database.h"
#include "global_vars.h"
#include "utils.h"

#include <cjson/cJSON.h>

#include <dirent.h>
#include <errno.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>

cJSON *get_temp_control_json() {
  cJSON *dto = cJSON_CreateObject();
  if (dto == NULL) {
    syslog(LOG_ERR, "cJSON_CreateObject() returns NULL");
    return NULL;
  }
  const size_t external_temps_size = pl.num_ext_sensors * 20 + 1;
  const size_t internal_temps_size = pl.num_int_sensors * 20 + 1;
  char external_temps_str[external_temps_size];
  char internal_temps_str[internal_temps_size];
  (void)concat_float_arr_to_cstr(pl.num_ext_sensors, pl.ext_temps,
                                 external_temps_size, external_temps_str);
  (void)concat_float_arr_to_cstr(pl.num_int_sensors, pl.int_temps,
                                 internal_temps_size, internal_temps_str);
  cJSON_AddItemToObject(dto, "external_temps",
                        cJSON_CreateString(external_temps_str));
  cJSON_AddItemToObject(dto, "internal_temps",
                        cJSON_CreateString(internal_temps_str));
  time_t now;
  time(&now);
  char dt_buffer[sizeof(SAMPLE_ISO_DT_STRING)];
  strftime(dt_buffer, sizeof dt_buffer, "%Y-%m-%d %H:%M:%S", localtime(&now));
  cJSON_AddItemToObject(dto, "record_time", cJSON_CreateString(dt_buffer));
  cJSON_AddItemToObject(dto, "fans_load", cJSON_CreateNumber(pl.fans_load));
  /* cJSON_AddItemToObject() transfers ownership from cJSON_Create...() to dto
     According to this link: https://github.com/DaveGamble/cJSON#printing */
  return dto;
}

cJSON *get_rack_door_states_json() {
  const size_t max_row_count = 6;
  int ids[max_row_count];
  char record_times[max_row_count][sizeof(SAMPLE_ISO_DT_STRING)];
  int states[max_row_count];
  cJSON *dto = cJSON_CreateObject();
  if (dto == NULL) {
    goto err_failure;
  }
  const ssize_t row_count = get_top_six_door_states(ids, record_times, states);
  cJSON *entries = cJSON_AddArrayToObject(dto, "data");
  if (entries == NULL) {
    goto err_failure;
  }
  for (ssize_t i = 0; i < row_count; ++i) {
    cJSON *entry = cJSON_CreateObject();
    if (cJSON_AddNumberToObject(entry, "record_id", ids[i]) == NULL ||
        cJSON_AddStringToObject(entry, "record_time", record_times[i]) ==
            NULL ||
        cJSON_AddNumberToObject(entry, "door_state", states[i]) == NULL) {
      goto err_failure;
    }
    cJSON_AddItemToArray(entries, entry);
  }
  return dto;
err_failure:
  syslog(LOG_ERR, "cJSON_CreateObject() returns NULL");
  cJSON_Delete(dto);
  return NULL;
}

char *read_file(const char *root_directory, const char *path,
                ssize_t *file_len) {
  *file_len = 0;
  FILE *fp;
  char *buffer = NULL;
  char file_path[PATH_MAX] = {0};
  strcat(file_path, root_directory);
  if (strlen(file_path) + strlen(path) < PATH_MAX - 2) {
    strcat(file_path, path);
  }
  // The tricky part is that C standard does NOT seems to mention what is
  // expected to happen if we pass a directory, not a filepath, to fopen()
  // Seems on some platforms, fopen() will not fail.
  DIR *dir = opendir(file_path);
  if (dir) {
    closedir(dir);
    goto err_is_dir;
  }

  fp = fopen(file_path, "rb");
  if (fp == NULL) {
    syslog(LOG_ERR, "fopen(%s) failed: %d(%s)", file_path, errno,
           strerror(errno));
    *file_len = 0;
    goto err_fopen;
  }

  (void)fseek(fp, 0, SEEK_END);
  *file_len = ftell(fp);
  if (*file_len == -1) {
    file_len = 0;
    syslog(LOG_ERR, "ftell() failed: %d(%s)", errno, strerror(errno));
    goto err_ftell;
  }
  (void)rewind(fp);

  buffer = malloc(*file_len);
  if (buffer == NULL) {
    syslog(LOG_ERR, "%d@%s: malloc(%ld) failed: %d(%s)", __LINE__, __FILE__,
           *file_len, errno, strerror(errno));
    *file_len = 0;
    goto err_malloc;
  }

  if ((ssize_t)fread(buffer, 1, *file_len, fp) != *file_len) {
    *file_len = 0;
    syslog(LOG_ERR, "fread() failed");
    free(buffer);
  }

err_malloc:
err_ftell:
  (void)fclose(fp);
err_fopen:
err_is_dir:
  return buffer;
}

int filename_compare(const void *a, const void *b) {
  return strcmp(*(const char **)a, *(const char **)b);
}

cJSON *get_images_list_json(const char *image_directory) {
  cJSON *dto = cJSON_CreateObject();
  if (dto == NULL) {
    syslog(LOG_ERR, "cJSON allocation failed");
    goto cJSON_CreateObject;
  }
  cJSON *json_filenames = cJSON_AddArrayToObject(dto, "data");
  if (json_filenames == NULL) {
    syslog(LOG_ERR, "cJSON_AddArrayToObject() failed");
    goto err_cJSON_AddArrayToObject;
  }
  ssize_t filenames_capacity = 1;
  ssize_t filenames_count = 0;
  char **filenames = malloc(sizeof(char *) * filenames_capacity);
  if (filenames == NULL) {
    syslog(LOG_ERR, "malloc() failed");
    goto err_malloc;
  }

  DIR *d = opendir(image_directory);
  struct dirent *dir;
  if (d) {
    while ((dir = readdir(d)) != NULL) {
      if (dir->d_name[0] != '.') {
        if (filenames_count >= filenames_capacity) {
          filenames_capacity *= 2;
          char **t = realloc(filenames, filenames_capacity * sizeof(char *));
          if (t == NULL) {
            syslog(LOG_ERR, "realloc() failed, abort reading more filenames");
            break;
          }
          filenames = t;
        }
        filenames[filenames_count] =
            malloc(sizeof(char) * strlen(dir->d_name) + 1);
        if (filenames[filenames_count] == NULL) {
          syslog(LOG_ERR, "malloc() failed, abort reading more filenames");
          break;
        }
        strcpy(filenames[filenames_count], dir->d_name);
        ++filenames_count;
      }
    }
    closedir(d);
    qsort(filenames, filenames_count, sizeof(char *), filename_compare);
    for (ssize_t i = 0; i < filenames_count; ++i) {
      // cJSON_CreateString() makes copy of the string.
      cJSON_AddItemToArray(json_filenames, cJSON_CreateString(filenames[i]));
      free(filenames[i]);
    }
  } else {
    syslog(LOG_ERR, "opendir() failed");
  }
  free(filenames);
  return dto;
err_malloc:
err_cJSON_AddArrayToObject:
  cJSON_Delete(dto);
cJSON_CreateObject:
  return NULL;
}
