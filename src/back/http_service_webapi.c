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

enum MHD_Result resp_404(struct MHD_Connection *conn) {
  char msg[PATH_MAX];
  snprintf(msg, PATH_MAX - 1, "Resource not found");
  syslog(LOG_WARNING, "%s", msg);
  struct MHD_Response *resp = MHD_create_response_from_buffer(
      strlen(msg), (void *)msg, MHD_RESPMEM_MUST_COPY);
  enum MHD_Result ret = MHD_queue_response(conn, MHD_HTTP_NOT_FOUND, resp);
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
  // const char *me = (const char *)cls;
  struct MHD_Response *resp = NULL;
  enum MHD_Result ret;
  char *user;
  char *pass = NULL;
  int fail;

  if (0 != strcmp(method, MHD_HTTP_METHOD_GET))
    return MHD_NO; /* unexpected method */
  if (&aptr != *ptr) {
    /* do never respond on first call */
    *ptr = &aptr;
    return MHD_YES;
  }
  *ptr = NULL; /* reset when done */

  user = MHD_basic_auth_get_username_password(conn, &pass);
  fail = ((user == NULL) || (0 != strcmp(user, http_auth_username)) ||
          (0 != strcmp(pass, http_auth_password)));
  MHD_free(user);
  MHD_free(pass);

  if (fail && 0) {
    const char msg[] = "Access denied";
    resp = MHD_create_response_from_buffer(strlen(msg), (void *)msg,
                                           MHD_RESPMEM_MUST_COPY);
    ret = MHD_queue_basic_auth_fail_response(conn, "PA Client Realm", resp);
    MHD_destroy_response(resp);
    return ret;
  }

  if (strcmp(url, "/health_check/") == 0) {
    const char msg[] = PROJECT_NAME " is up and running";
    resp = MHD_create_response_from_buffer(strlen(msg), (void *)msg,
                                           MHD_RESPMEM_MUST_COPY);
    ret = MHD_queue_response(conn, MHD_HTTP_OK, resp);
    MHD_destroy_response(resp);
    return ret;
  }
  if (strcmp(url, "/get_logged_in_user/") == 0) {
    char username_json[strlen("{\"data\":\"%s\"}") + 1 +
                       strlen(http_auth_username) + 1];
    snprintf(username_json, sizeof(username_json), "{\"data\":\"%s\"}",
             http_auth_username);
    resp = MHD_create_response_from_buffer(
        strlen(username_json), (void *)username_json, MHD_RESPMEM_MUST_COPY);
    MHD_add_response_header(resp, "Content-Type", "application/json;");
    MHD_add_response_header(resp, "Access-Control-Allow-Origin", "*");
    ret = MHD_queue_response(conn, MHD_HTTP_OK, resp);
    MHD_destroy_response(resp);
    return ret;
  }
  if (strcmp(url, "/get_temp_control_json/") == 0) {
    cJSON *dto = get_temp_control_json();

    const char *dto_json_str = cJSON_PrintUnformatted(dto);
    resp = MHD_create_response_from_buffer(
        strlen(dto_json_str), (void *)dto_json_str, MHD_RESPMEM_MUST_FREE);
    MHD_add_response_header(resp, "Content-Type", "application/json");
    MHD_add_response_header(resp, "Access-Control-Allow-Origin", "*");
    ret = MHD_queue_response(conn, MHD_HTTP_OK, resp);
    MHD_destroy_response(resp);
    cJSON_Delete(dto);
    return ret;
  }
  if (strcmp(url, "/get_rack_door_states_json/") == 0) {
    cJSON *dto = get_rack_door_states_json();
    const char *dto_json_str = cJSON_PrintUnformatted(dto);
    resp = MHD_create_response_from_buffer(
        strlen(dto_json_str), (void *)dto_json_str, MHD_RESPMEM_MUST_FREE);
    MHD_add_response_header(resp, "Content-Type", "application/json");
    MHD_add_response_header(resp, "Access-Control-Allow-Origin", "*");
    ret = MHD_queue_response(conn, MHD_HTTP_OK, resp);
    MHD_destroy_response(resp);
    cJSON_Delete(dto);
    return ret;
  }
  if (strcmp(url, "/get_images_jpg/") == 0) {
    const char *image_name =
        MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "imageName");
    if (image_name != NULL) {
      resp = MHD_create_response_from_buffer(
          strlen(image_name), (void *)image_name, MHD_RESPMEM_MUST_COPY);
      ret = MHD_queue_response(conn, MHD_HTTP_OK, resp);
    } else {
      const char err_msg[] = "Not found!";
      resp = MHD_create_response_from_buffer(strlen(err_msg), (void *)err_msg,
                                             MHD_RESPMEM_MUST_COPY);
      ret = MHD_queue_response(conn, MHD_HTTP_NOT_FOUND, resp);
    }
    MHD_destroy_response(resp);
    return ret;
  }

  return resp_404(conn);
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
      !cJSON_IsString(json_auth_password)) {
    syslog(LOG_ERR, "Malformed JSON config file");
    return NULL;
  }

  http_auth_username = json_auth_username->valuestring;
  http_auth_password = json_auth_password->valuestring;

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
