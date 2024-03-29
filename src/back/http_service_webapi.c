#include "http_service_webapi.h"
#include "database.h"
#include "http_service_application.h"

#include <arpa/inet.h>
#include <cjson/cJSON.h>
#include <errno.h>
#include <linux/limits.h>
#include <microhttpd.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <syslog.h>

#define SSL_FILE_BUFF_SIZE 8192
const char *http_auth_username;
const char *http_auth_password;
const char *image_directory;
const char *static_file_root_directory;
const char *advertised_addr;

int detect_directory_traversal(const char *path) {
  // Check for use of "../" to traverse up the directory hierarchy
  if (strstr(path, "../") != NULL) {
    return 1;
  }

  // Check for use of "..\" in Windows style paths
  if (strstr(path, "..\\") != NULL) {
    return 1;
  }

  // Check for use of "/../" in Linux/Unix style paths
  if (strstr(path, "/../") != NULL) {
    return 1;
  }

  // Path passed all checks
  return 0;
}

int http_authentication_successful(struct MHD_Connection *conn) {
  char *user;
  char *pass = NULL;

  user = MHD_basic_auth_get_username_password(conn, &pass);
  int fail = ((user == NULL) || (0 != strcmp(user, http_auth_username)) ||
              (0 != strcmp(pass, http_auth_password)));
  MHD_free(user);
  MHD_free(pass);
  return !fail;
}

enum MHD_Result handler_get_logged_in_user(struct MHD_Connection *conn) {
  enum MHD_Result ret;
  struct MHD_Response *resp = NULL;
  char username_json[strlen("{\"data\":\"%s\"}") + 1 +
                     strlen(http_auth_username) + 1];
  snprintf(username_json, sizeof(username_json), "{\"data\":\"%s\"}",
           http_auth_username);
  resp = MHD_create_response_from_buffer(
      strlen(username_json), (void *)username_json, MHD_RESPMEM_MUST_COPY);
  MHD_add_response_header(resp, "Content-Type", "application/json;");
  ret = MHD_queue_response(conn, MHD_HTTP_OK, resp);
  MHD_destroy_response(resp);
  return ret;
}
enum MHD_Result handler_get_temp_control_json(struct MHD_Connection *conn) {
  cJSON *dto = get_temp_control_json();
  enum MHD_Result ret;
  struct MHD_Response *resp = NULL;
  const char *dto_json_str = cJSON_PrintUnformatted(dto);
  resp = MHD_create_response_from_buffer(
      strlen(dto_json_str), (void *)dto_json_str, MHD_RESPMEM_MUST_FREE);
  MHD_add_response_header(resp, "Content-Type", "application/json");
  ret = MHD_queue_response(conn, MHD_HTTP_OK, resp);
  MHD_destroy_response(resp);
  cJSON_Delete(dto);
  return ret;
}

enum MHD_Result handler_get_rack_door_states_json(struct MHD_Connection *conn) {
  cJSON *dto = get_rack_door_states_json();
  enum MHD_Result ret;
  struct MHD_Response *resp = NULL;
  const char *dto_json_str = cJSON_PrintUnformatted(dto);
  resp = MHD_create_response_from_buffer(
      strlen(dto_json_str), (void *)dto_json_str, MHD_RESPMEM_MUST_FREE);
  MHD_add_response_header(resp, "Content-Type", "application/json");
  ret = MHD_queue_response(conn, MHD_HTTP_OK, resp);
  MHD_destroy_response(resp);
  cJSON_Delete(dto);
  return ret;
}

enum MHD_Result handler_get_images_jpg(struct MHD_Connection *conn) {
  enum MHD_Result ret;
  struct MHD_Response *resp = NULL;
  const char *image_name =
      MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "imageName");
  if (image_name == NULL) {
    const char err_msg[] = "Not found!";
    resp = MHD_create_response_from_buffer(strlen(err_msg), (void *)err_msg,
                                           MHD_RESPMEM_MUST_COPY);
    ret = MHD_queue_response(conn, MHD_HTTP_NOT_FOUND, resp);
    goto finalize_image_resp;
  }
  if (detect_directory_traversal(image_name) == 1) {
    const char err_msg[] = "Root traversal attempt!";
    resp = MHD_create_response_from_buffer(strlen(err_msg), (void *)err_msg,
                                           MHD_RESPMEM_MUST_COPY);
    ret = MHD_queue_response(conn, MHD_HTTP_BAD_REQUEST, resp);

    goto finalize_image_resp;
  }
  ssize_t image_length = 0;
  char *image_buffer = read_file(image_directory, image_name, &image_length);
  if (image_length > 0) {
    resp = MHD_create_response_from_buffer(image_length, (void *)image_buffer,
                                           MHD_RESPMEM_MUST_FREE);
    ret = MHD_queue_response(conn, MHD_HTTP_OK, resp);
    goto finalize_image_resp;
  }
  const char err_msg[] = "Error opening image file";
  resp = MHD_create_response_from_buffer(strlen(err_msg), (void *)err_msg,
                                         MHD_RESPMEM_MUST_COPY);
  ret = MHD_queue_response(conn, MHD_HTTP_BAD_REQUEST, resp);
finalize_image_resp:
  MHD_destroy_response(resp);
  return ret;
}

enum MHD_Result handler_get_images_list_json(struct MHD_Connection *conn) {
  cJSON *dto = get_images_list_json(image_directory);
  enum MHD_Result ret;
  struct MHD_Response *resp = NULL;
  const char *dto_json_str = cJSON_PrintUnformatted(dto);
  resp = MHD_create_response_from_buffer(
      strlen(dto_json_str), (void *)dto_json_str, MHD_RESPMEM_MUST_FREE);
  MHD_add_response_header(resp, "Content-Type", "application/json");
  ret = MHD_queue_response(conn, MHD_HTTP_OK, resp);
  MHD_destroy_response(resp);
  cJSON_Delete(dto);
  return ret;
}

enum MHD_Result handler_root(struct MHD_Connection *conn) {
  enum MHD_Result ret;
  struct MHD_Response *resp = NULL;
  resp = MHD_create_response_from_buffer(0, NULL, MHD_RESPMEM_MUST_FREE);
  char main_page_url[PATH_MAX] = {0};
  strcpy(main_page_url, advertised_addr);
  strcat(main_page_url, "/html/index.html");
  MHD_add_response_header(resp, "Location", main_page_url);
  ret = MHD_queue_response(conn, MHD_HTTP_MOVED_PERMANENTLY, resp);
  MHD_destroy_response(resp);
  return ret;
}

enum MHD_Result handler_auth_failed(struct MHD_Connection *conn) {
  struct MHD_Response *resp = NULL;
  enum MHD_Result ret;
  const char msg[] = "Access denied";
  resp = MHD_create_response_from_buffer(strlen(msg), (void *)msg,
                                         MHD_RESPMEM_MUST_COPY);
  ret = MHD_queue_basic_auth_fail_response(conn, "Rack daemon Realm", resp);
  MHD_destroy_response(resp);
  return ret;
}

enum MHD_Result handler_fallback(struct MHD_Connection *conn, const char *url) {
  enum MHD_Result ret;
  struct MHD_Response *resp = NULL;
  ssize_t file_length = 0;
  char *file_buffer = read_file(static_file_root_directory, url, &file_length);
  if (file_length > 0) {
    resp = MHD_create_response_from_buffer(file_length, (void *)file_buffer,
                                           MHD_RESPMEM_MUST_FREE);
    ret = MHD_queue_response(conn, MHD_HTTP_OK, resp);
    goto finalize_file_resp;
  }
  const char err_msg[] = "File not found";
  resp = MHD_create_response_from_buffer(strlen(err_msg), (void *)err_msg,
                                         MHD_RESPMEM_MUST_COPY);
  ret = MHD_queue_response(conn, MHD_HTTP_NOT_FOUND, resp);
finalize_file_resp:
  MHD_destroy_response(resp);
  return ret;
}

// enum MHD_Result *MHD_AccessHandlerCallback (void *cls, struct MHD_Connection
// * connection, const char *url, const char *method, const char *version, const
// char *upload_data, size_t *upload_data_size, void **con_cls)
enum MHD_Result
request_handler(__attribute__((unused)) void *cls, struct MHD_Connection *conn,
                const char *url, const char *method,
                __attribute__((unused)) const char *version,
                __attribute__((unused)) const char *upload_data,
                __attribute__((unused)) size_t *upload_data_size, void **ptr) {
  static int aptr;
  if (0 != strcmp(method, MHD_HTTP_METHOD_GET))
    return MHD_NO; /* unexpected method */
  if (&aptr != *ptr) {
    /* do never respond on first call */
    *ptr = &aptr;
    return MHD_YES;
  }
  *ptr = NULL; /* reset when done */

  if (!http_authentication_successful(conn)) {
    return handler_auth_failed(conn);
  }
  if (strcmp(url, "/get_logged_in_user/") == 0) {
    return handler_get_logged_in_user(conn);
  }
  if (strcmp(url, "/get_temp_control_json/") == 0) {
    return handler_get_temp_control_json(conn);
  }
  if (strcmp(url, "/get_rack_door_states_json/") == 0) {
    return handler_get_rack_door_states_json(conn);
  }
  if (strcmp(url, "/get_images_jpg/") == 0) {
    return handler_get_images_jpg(conn);
  }
  if (strcmp(url, "/get_images_list_json/") == 0) {
    return handler_get_images_list_json(conn);
  }
  if (strcmp(url, "/") == 0) {
    return handler_root(conn);
  }
  return handler_fallback(conn, url);
}

int load_ssl_key_or_crt(const char *path, char **out_content) {
  FILE *fp;
  int retval = 0;
  fp = fopen(path, "rb");
  if (fp == NULL) {
    syslog(LOG_ERR, "Failed to fopen() %s", path);
    retval = -1;
    goto err_fopen;
  }

  size_t bytes_read = fread(out_content, sizeof(char), SSL_FILE_BUFF_SIZE, fp);
  if (bytes_read > 0) {
  } else if (feof(fp)) {
    syslog(LOG_ERR, "feof() error while reading from [%s]", path);
  } else if (ferror(fp)) {
    syslog(LOG_ERR, "ferror() while` reading from [%s]", path);
    retval = -1;
    goto err_ferror;
  }
err_ferror:
  fclose(fp);
err_fopen:
  return retval;
}

struct MHD_Daemon *init_mhd(const cJSON *json) {
  const cJSON *json_ssl = cJSON_GetObjectItemCaseSensitive(json, "ssl");
  const cJSON *json_image_directory =
      cJSON_GetObjectItemCaseSensitive(json, "image_directory");
  const cJSON *json_static_file_root_directory =
      cJSON_GetObjectItemCaseSensitive(json, "static_file_root_directory");
  const cJSON *json_advertised_addr =
      cJSON_GetObjectItemCaseSensitive(json, "advertised_addr");
  const cJSON *json_ssl_crt_path =
      cJSON_GetObjectItemCaseSensitive(json_ssl, "crt_path");
  const cJSON *json_ssl_key_path =
      cJSON_GetObjectItemCaseSensitive(json_ssl, "key_path");

  const cJSON *json_auth = cJSON_GetObjectItemCaseSensitive(json, "auth");
  const cJSON *json_auth_username =
      cJSON_GetObjectItemCaseSensitive(json_auth, "username");
  const cJSON *json_auth_password =
      cJSON_GetObjectItemCaseSensitive(json_auth, "password");

  if (json == NULL ||
      !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(json, "interface")) ||
      !cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(json, "port")) ||
      !cJSON_IsString(
          cJSON_GetObjectItemCaseSensitive(json, "advertised_addr")) ||
      !cJSON_IsString(json_ssl_crt_path) ||
      !cJSON_IsString(json_ssl_key_path) ||
      !cJSON_IsString(json_auth_username) ||
      !cJSON_IsString(json_auth_password) ||
      !cJSON_IsString(json_image_directory) ||
      !cJSON_IsString(json_static_file_root_directory)) {
    syslog(LOG_ERR, "Malformed JSON config file");
    return NULL;
  }

  http_auth_username = json_auth_username->valuestring;
  http_auth_password = json_auth_password->valuestring;
  image_directory = json_image_directory->valuestring;
  static_file_root_directory = json_static_file_root_directory->valuestring;
  advertised_addr = json_advertised_addr->valuestring;

  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port =
      htons(cJSON_GetObjectItemCaseSensitive(json, "port")->valueint);
  if (inet_pton(
          AF_INET,
          cJSON_GetObjectItemCaseSensitive(json, "interface")->valuestring,
          &(server_addr.sin_addr)) < 0) {
    syslog(LOG_ERR, "inet_pton() error: %d(%s)", errno, strerror(errno));
    return NULL;
  }
  char ssl_key[SSL_FILE_BUFF_SIZE], ssl_crt[SSL_FILE_BUFF_SIZE];
  load_ssl_key_or_crt(json_ssl_crt_path->valuestring, (char **)(&ssl_crt));
  load_ssl_key_or_crt(json_ssl_key_path->valuestring, (char **)(&ssl_key));
  struct MHD_Daemon *daemon;
  // clang-format off
  // struct MHD_Daemon * MHD_start_daemon (unsigned int flags, unsigned short port, MHD_AcceptPolicyCallback apc, void *apc_cls, MHD_AccessHandlerCallback dh, void *dh_cls, ...)
  daemon = MHD_start_daemon(
    MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_DEBUG | MHD_USE_TLS,
    server_addr.sin_port, NULL,
    NULL, &request_handler, "",
    MHD_OPTION_CONNECTION_TIMEOUT, 256,
    MHD_OPTION_SOCK_ADDR, (struct sockaddr *)&server_addr,
    MHD_OPTION_HTTPS_MEM_CERT, ssl_crt,
    MHD_OPTION_HTTPS_MEM_KEY, ssl_key,
    MHD_OPTION_END);
  // clang-format on
  syslog(LOG_INFO, "HTTP service listening on https://%s:%d",
         cJSON_GetObjectItemCaseSensitive(json, "interface")->valuestring,
         cJSON_GetObjectItemCaseSensitive(json, "port")->valueint);
  return daemon;
}
