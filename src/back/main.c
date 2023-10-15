#include "7seg_display.h"
#include "database.h"
#include "event_loops.h"
#include "global_vars.h"
#include "http_service_application.h"
#include "http_service_webapi.h"

#include <cjson/cJSON.h>
#include <pigpio.h>

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <limits.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syslog.h>
#include <syslog.h>
#include <unistd.h>

#define BAD_TEMPERATURE 65535

struct Payload pl;
volatile sig_atomic_t ev_flag = 0;

void signal_handler(int signum) {
  ev_flag = 1;
  char msg[] = "Signal [  ] caught\n";
  msg[8] = '0' + signum / 10;
  msg[9] = '0' + signum % 10;
  write(STDERR_FILENO, msg, strlen(msg));
}

void print_usage(const char *binary_name) {

  printf("Usage: %s [OPTION]\n\n", binary_name);

  printf("Options:\n"
         "  --help,        -h        Display this help and exit\n"
         "  --config-path, -c        Path of JSON format configuration file\n");
}

void load_sensors(const cJSON *json) {
  for (size_t i = 0; i < pl.num_int_sensors; ++i) {
    pl.int_temps[i] = BAD_TEMPERATURE;
  }
  for (size_t i = 0; i < pl.num_ext_sensors; ++i) {
    pl.ext_temps[i] = BAD_TEMPERATURE;
  }
  cJSON *s;
  syslog(LOG_INFO, "Loading external sensors:");
  cJSON_ArrayForEach(
      s, cJSON_GetObjectItemCaseSensitive(json, "external_sensors")) {
    if (cJSON_IsString(s)) {
      pl.ext_sensor_paths[pl.num_ext_sensors] = strdup(s->valuestring);
      syslog(LOG_INFO, "%s", pl.ext_sensor_paths[pl.num_ext_sensors]);
      ++pl.num_ext_sensors;
      if (pl.num_ext_sensors >= MAX_SENSORS) {
        fprintf(stderr, "Program supports up to %d external sensors only",
                MAX_SENSORS);
        abort();
      }
    }
  }
  syslog(LOG_INFO, "%lu external sensors loaded", pl.num_ext_sensors);

  syslog(LOG_INFO, "Loading internal sensors");
  cJSON_ArrayForEach(
      s, cJSON_GetObjectItemCaseSensitive(json, "internal_sensors")) {
    if (cJSON_IsString(s)) {
      pl.int_sensor_paths[pl.num_int_sensors] = strdup(s->valuestring);
      syslog(LOG_INFO, "%s", pl.int_sensor_paths[pl.num_int_sensors]);
      ++pl.num_int_sensors;
      if (pl.num_int_sensors >= MAX_SENSORS) {
        fprintf(stderr, "Program supports up to %d external sensors only",
                MAX_SENSORS);
        abort();
      }
    }
  }
  syslog(LOG_INFO, "%lu internal sensors loaded", pl.num_int_sensors);
}

const char *parse_args(int argc, char *argv[]) {
  static struct option long_options[] = {
      {"config-path", required_argument, 0, 'c'},
      {"help", optional_argument, 0, 'h'},
      {0, 0, 0, 0}};
  int opt, option_idx = 0;
  while ((opt = getopt_long(argc, argv, "c:h", long_options, &option_idx)) !=
         -1) {
    switch (opt) {
    case 'c':
      return strdup(optarg);
    }
  }
  print_usage(argv[0]);
  _exit(1);
}

cJSON *read_config_file(const char *config_path) {
  FILE *fp = fopen(config_path, "r");
  if (fp == NULL) {
    fprintf(stderr, "Error: Unable to open the file [%s]\n", config_path);
    _exit(1);
  }
  fseek(fp, 0, SEEK_END);
  size_t fsize = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  char *json_buf = malloc(fsize + 1);
  if (json_buf == NULL) {
    fprintf(stderr, "Error: Unable to open malloc() memory for file [%s]\n",
            config_path);
    _exit(1);
  }
  fread(json_buf, fsize, 1, fp);
  fclose(fp);
  cJSON *json = cJSON_Parse(json_buf);
  if (json == NULL) {
    fprintf(stderr, "Error: Unable to parse [%s] as JSON file\n", config_path);
    _exit(1);
  }
  syslog(LOG_INFO, "Content of [%s]:\n%s", config_path, cJSON_Print(json));
  return json;
}

void save_temp_to_payload(char *sensors[], const size_t sensor_count,
                          int32_t *temps, int32_t *temp) {
  char buf[PATH_MAX];
  int fd;
  size_t valid_temps_count = 0;
  int _temp = 0;
  for (size_t i = 0; i < sensor_count && !ev_flag; ++i) {
    sleep(1);
    // takes around 1 sec to read value from one sensor.
    fd = open(sensors[i], O_RDONLY);
    if (fd >= 0) {
      if (read(fd, buf, sizeof(buf)) > 0) {
        char *temp_str = strstr(buf, "t=") + 2;
        sscanf(temp_str, "%d", &(temps[i]));
      }
      close(fd);
      _temp += temps[i];
      ++valid_temps_count;
    } else {
      temps[i] = BAD_TEMPERATURE;
      syslog(LOG_ERR,
             "Unable to open device at [%s], errno: %d(%s), skipped this "
             "read attempt.",
             sensors[i], errno, strerror(errno));
    }
    sleep(1);
  }
  if (valid_temps_count > 0) {
    _temp /= valid_temps_count;
  } else {
    _temp = 0;
  }
  // An intermediary variable canNOT be removed or there will be race condition
  *temp = _temp;
}

int install_signal_handler() {
  // This design canNOT handle more than 99 signal types
  if (_NSIG > 99) {
    syslog(LOG_ERR, "signal_handler() can't handle more than 99 signals");
    return -1;
  }
  struct sigaction act;
  // Initialize the signal set to empty, similar to memset(0)
  if (sigemptyset(&act.sa_mask) == -1) {
    syslog(LOG_ERR, "sigemptyset() failed");
    return -1;
  }
  act.sa_handler = signal_handler;
  /*
  * SA_RESETHAND means we want our signal_handler() to intercept the signal
  once. If a signal is sent twice, the default signal handler will be used
  again. `man sigaction` describes more possible sa_flags.
  * In this particular case, we should not enable SA_RESETHAND, mainly
  due to the issue that if a child process is kill, multiple SIGPIPE will
  be invoked consecutively, breaking the program.
  * Without setting SA_RESETHAND, catching SIGSEGV is usually a bad idea.
  The issue is, if an instruction results in segfault, SIGSEGV handler is
  called, then the very same instruction will be repeated, triggering
  segfault again. */
  // act.sa_flags = SA_RESETHAND;
  act.sa_flags = 0;
  if (sigaction(SIGINT, &act, 0) + sigaction(SIGABRT, &act, 0) +
          sigaction(SIGQUIT, &act, 0) + sigaction(SIGTERM, &act, 0) <
      0) {

    /* Could miss some error if more than one sigaction() fails. However,
    given that the program will quit if one sigaction() fails, this
    is not considered an issue */
    syslog(LOG_ERR, "sigaction() failed");
    return -1;
  }
  return 0;
}

int main(int argc, char *argv[]) {
  int retval = 0;
  pthread_t tids[5] = {0};
  cJSON *json = read_config_file(parse_args(argc, argv));
  (void)openlog("rd", LOG_PID | LOG_CONS, 0);
  syslog(LOG_INFO, "rd started\n");
  if (prepare_database() != 0) {
    retval = -1;
    goto err_prepare_database;
  }
  if (gpioInitialise() < 0) {
    syslog(LOG_ERR, "gpioInitialise() failed, program will quit");
    retval = -1;
    goto err_gpioInitialise;
  }
  if (install_signal_handler() < 0) {
    syslog(LOG_ERR, "install_signal_handler() failed, program will quit");
    retval = -1;
    goto err_install_signal_handler;
  }

  struct MHD_Daemon *httpd =
      init_mhd(cJSON_GetObjectItemCaseSensitive(json, "http_service"));
  if (httpd == NULL) {
    syslog(LOG_ERR, "init_mhd() failed");
    goto err_init_mhd;
  }
  load_sensors(json);
  if (pthread_create(&tids[0], NULL, ev_get_temp_from_sensors, NULL) != 0 ||
      pthread_create(&tids[1], NULL, ev_apply_fans_load, NULL) != 0 ||
      pthread_create(&tids[2], NULL, ev_monitor_rack_door, NULL) != 0 ||
      pthread_create(&tids[3], NULL, ev_set_7seg_display, NULL) != 0) {
    syslog(LOG_ERR, "Failed to create essential threads, program will quit");
    ev_flag = 1;
    retval = -1;
    // We don't use goto to skip pthread_join() here and we set ev_flag to 1
    // only--if some of pthread_create()s are successful and some of them
    // failed, pthread_join() will return error on these failed threadss, but
    // this won't harm. On the other hand, if we just skip pthread_join(), some
    // successfully pthread_create()ed threads will be left neither
    // pthread_join()ed nor pthread_detach()ed, which is not POSIX-compliant and
    // leaks resources
  }
  for (size_t i = 0; i < sizeof(tids) / sizeof(tids[0]); ++i) {
    pthread_join(tids[i], NULL);
  }
  MHD_stop_daemon(httpd);
err_init_mhd:
  /* Stop DMA, release resources */
  gpioTerminate();
  syslog(LOG_INFO, "main() quits gracefully.");
err_install_signal_handler:
err_gpioInitialise:
err_prepare_database:
  closelog();
  cJSON_Delete(json);
  return retval;
}
