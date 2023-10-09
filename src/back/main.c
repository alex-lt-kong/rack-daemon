#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <limits.h>
#include <linux/limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syslog.h>
#include <syslog.h>
#include <unistd.h>

#include <curl/curl.h>
#include <pigpio.h>
#include <sqlite3.h>

#include "7seg.c"

#define BAD_TEMPERATURE 65535
#define MAX_SENSORS 16

struct Payload {
  /* The program relies on glibc's implicit guarantee to achieve "lock-free"
   * issue:
   * https://www.gnu.org/software/libc/manual/html_node/Atomic-Types.html.
   * As a result, all members of Payload have to be either char or int;
   * otherwise, we may need to use mutex to aovid data corruption*/
  int32_t int_temps[MAX_SENSORS];
  int32_t ext_temps[MAX_SENSORS];
  int32_t int_temp;
  int32_t ext_temp;
  char *int_sensors[MAX_SENSORS];
  char *ext_sensors[MAX_SENSORS];
  size_t num_ext_sensors;
  size_t num_int_sensors;
  int32_t fans_load;
} pl;

volatile sig_atomic_t ev_flag = 0;
char db_path[PATH_MAX];

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
         "  --help,            -h        Display this help and exit\n"
         "  --ext-sensor-path, -e        Path of external sensor device, "
         "repeat the argument to read from multiple devices\n"
         "  --int-sensor-path, -i        Path of internal sensor device, "
         "repeat the argument to read from multiple devices\n");
}

void parse_args(int argc, char *argv[]) {
  static struct option long_options[] = {
      {"ext-sensor-path", required_argument, 0, 'e'},
      {"int-sensor-path", required_argument, 0, 'i'},
      {"help", optional_argument, 0, 'h'},
      {0, 0, 0, 0}};
  int opt, option_idx = 0;
  pl.num_ext_sensors = 0;
  pl.num_int_sensors = 0;
  // Parse the options using getopt
  while ((opt = getopt_long(argc, argv, "e:i:h", long_options, &option_idx)) !=
         -1) {
    switch (opt) {
    case 'e':
      if (pl.num_ext_sensors >= MAX_SENSORS) {
        fprintf(stderr, "Support up to %d external sensors\n", MAX_SENSORS);
        abort();
      }
      pl.ext_sensors[pl.num_ext_sensors] = strdup(optarg);
      if (pl.ext_sensors[pl.num_ext_sensors] == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        abort();
      }
      ++pl.num_ext_sensors;
      break;
    case 'i':
      if (pl.num_int_sensors >= MAX_SENSORS) {
        fprintf(stderr, "Support up to %d internal sensors\n", MAX_SENSORS);
        abort();
      }
      pl.int_sensors[pl.num_int_sensors] = strdup(optarg);
      if (pl.int_sensors[pl.num_int_sensors] == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        abort();
      }
      ++pl.num_int_sensors;
      break;
    case 'h':
      print_usage(argv[0]);
      abort();
    }
  }
  if (pl.num_int_sensors == 0 || pl.num_ext_sensors == 0) {
    print_usage(argv[0]);
    abort();
  }
  syslog(LOG_INFO, "External sensors:");
  for (size_t i = 0; i < pl.num_ext_sensors; ++i) {
    syslog(LOG_INFO, "%s", pl.ext_sensors[i]);
  }
  syslog(LOG_INFO, "Internal sensors:");
  for (size_t i = 0; i < pl.num_int_sensors; ++i) {
    syslog(LOG_INFO, "%s", pl.int_sensors[i]);
  }
}

int prepare_database() {
  int retval = 0;
  sqlite3 *db;
  snprintf(db_path, PATH_MAX, "%s", getenv("RD_DB_DIR"));
  const char sql_create_stmts[][512] = {
      "CREATE TABLE IF NOT EXISTS door_state"
      "("
      "  [record_id] INTEGER PRIMARY KEY AUTOINCREMENT,"
      "  [record_time] TEXT,"
      "  [door_state] INTEGER"
      ")",
      "CREATE TABLE IF NOT EXISTS temp_control"
      "("
      "  [record_id] INTEGER PRIMARY KEY AUTOINCREMENT,"
      "  [record_time] TEXT,"
      "  [external_temps] TEXT,"
      "  [internal_temps] TEXT,"
      "  [fans_load] INTEGER"
      ")"};
  char *sqlite_err_msg = NULL;
  const size_t table_count =
      sizeof(sql_create_stmts) / sizeof(sql_create_stmts[0]);
  if (sqlite3_open(db_path, &db) != SQLITE_OK) {
    syslog(LOG_ERR, "Cannot open database [%s]: %s. INSERT will be skipped",
           db_path, sqlite3_errmsg(db));
    retval = -1;
    goto err_sqlite3_open;
  }
  for (size_t i = 0; i < table_count; ++i) {
    if (sqlite3_exec(db, sql_create_stmts[i], NULL, NULL, &sqlite_err_msg) !=
        SQLITE_OK) {
      syslog(LOG_ERR, "SQL error: %s. SQL statement [%s] is not successful.",
             sqlite_err_msg, sql_create_stmts[i]);
      sqlite3_free(sqlite_err_msg);
    }
  }
err_sqlite3_open:
  if (sqlite3_close(db) != SQLITE_OK) {
    syslog(LOG_ERR, "sqlite3_close() failed.");
  }
  syslog(LOG_INFO, "%zu tables @ %s prepared", table_count, db_path);
  return retval;
}

void *ev_monitor_rack_door() {
  syslog(LOG_INFO, "ev_monitor_rack_door() started.");
  const int pin_pos = 19;
  const int pin_neg = 16;

  const char sql_insert[] = "INSERT INTO door_state"
                            "(record_time, door_state) "
                            "VALUES(?, ?);";
  time_t now;
  sqlite3 *db;

  gpioSetMode(pin_pos, PI_OUTPUT);
  gpioSetMode(pin_neg, PI_INPUT);
  gpioWrite(pin_pos, PI_HIGH);
  gpioSetPullUpDown(pin_neg, PI_PUD_UP);

  // false -> circuit opened -> door opened
  // true -> circuit closed -> door closed
  bool last_status = true;
  bool current_status = true;
  size_t status_count = 0;
  while (!ev_flag) {
    usleep(1000 * 1000); // i.e., 1sec
    current_status = gpioRead(pin_neg);
    if (current_status != last_status) {
      ++status_count;
      if (status_count < 5) {
        continue;
      }
      last_status = current_status;

      int rc = sqlite3_open(db_path, &db);
      if (rc != SQLITE_OK) {
        syslog(LOG_ERR, "Cannot open database [%s]: %s. INSERT skipped",
               db_path, sqlite3_errmsg(db));
        goto err_sqlite3_open;
      }
      sqlite3_stmt *stmt;
      if (sqlite3_prepare_v2(db, sql_insert, 512, &stmt, NULL) != SQLITE_OK) {
        syslog(LOG_ERR, "sqlite3_prepare_v2() failed: %s. INSERT skipped",
               sqlite3_errmsg(db));
        goto err_sqlite3_prepare;
      }
      time(&now);
      char buf[sizeof("1970-01-01 00:00:00")];
      strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", localtime(&now));
      /* here we try to be consistent with common sense:
        1 means "triggered" and thus "opened"*/
      if (sqlite3_bind_text(stmt, 1, buf, -1, NULL) != SQLITE_OK ||
          sqlite3_bind_int(stmt, 2, !current_status) != SQLITE_OK) {
        syslog(LOG_ERR, "sqlite3_bind_...() failed: %s. INSERT skipped",
               sqlite3_errmsg(db));
        goto err_sqlite3_bind;
      }
      if (sqlite3_step(stmt) != SQLITE_OK) {
        syslog(LOG_ERR, "sqlite3_step() failed: %s. INSERT skipped",
               sqlite3_errmsg(db));
      }
    err_sqlite3_bind:
      rc = sqlite3_finalize(stmt);
      if (rc != SQLITE_OK) {
        syslog(LOG_ERR, "SQL error: %d. INSERT is not successful.\n", rc);
        sqlite3_close(db);
        continue;
      }
    err_sqlite3_prepare:
    err_sqlite3_open:
      sqlite3_close(db);
    } else {
      status_count = 0;
    }
  }

  syslog(LOG_INFO, "ev_monitor_rack_door() quits gracefully.");
  return NULL;
}

int write_int_arr_to_cstr(const size_t arr_size, const int32_t *arr,
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

void save_data_to_db() {
  const char *sql_insert =
      "INSERT INTO temp_control"
      "(record_time, external_temps, internal_temps, fans_load) "
      "VALUES(?, ?, ?, ?);";
  time_t now;
  sqlite3 *db;
  char ext_temps[pl.num_ext_sensors * 6];
  char int_temps[pl.num_ext_sensors * 6];

  if (sqlite3_open(db_path, &db) != SQLITE_OK) {
    syslog(LOG_ERR, "Cannot open database [%s]: %s. INSERT skipped", db_path,
           sqlite3_errmsg(db));
    goto err_sqlite3_open;
  }
  sqlite3_stmt *stmt = NULL;
  if (sqlite3_prepare_v2(db, sql_insert, 512, &stmt, NULL) != SQLITE_OK) {
    syslog(LOG_ERR, "sqlite3_prepare_v2() error: %s, INSERT skipped",
           sqlite3_errmsg(db));
    goto err_sqlite3_prepare;
  }

  time(&now);
  char buf[sizeof("1970-01-01 00:00:00")];
  strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", localtime(&now));

  if (write_int_arr_to_cstr(pl.num_ext_sensors, pl.ext_temps, ext_temps) != 0 ||
      write_int_arr_to_cstr(pl.num_int_sensors, pl.int_temps, int_temps) != 0) {
    syslog(LOG_ERR, "write_int_arr_to_cstr() failed. INSERT skipped");
    goto err_temps_str;
  }

  if (sqlite3_bind_text(stmt, 1, buf, -1, NULL) != SQLITE_OK ||
      sqlite3_bind_text(stmt, 2, ext_temps, -1, NULL) != SQLITE_OK ||
      sqlite3_bind_text(stmt, 3, int_temps, -1, NULL) != SQLITE_OK ||
      sqlite3_bind_int(stmt, 4, pl.fans_load) != SQLITE_OK) {
    syslog(LOG_ERR, "sqlite3_bind_...() failed. INSERT skipped");
    goto err_sqlite_bind;
  }
  // sqlite3_step() "evaluates an SQL statement"
  if (sqlite3_step(stmt) != SQLITE_OK) {
    syslog(LOG_ERR, "sqlite3_step() failed. INSERT skipped");
  }
err_temps_str:
err_sqlite_bind:
  if (sqlite3_finalize(stmt) != SQLITE_OK) {
    syslog(LOG_ERR, "sqlite3_finalize() failed. INSERT skipped");
  }
err_sqlite3_prepare:
err_sqlite3_open:
  // Whether or not an error occurs when it is opened, resources associated
  // with the database connection handle should be released by passing it to
  // sqlite3_close() when it is no longer required.
  if (sqlite3_close(db) != SQLITE_OK) {
    syslog(LOG_ERR, "sqlite3_close() failed.");
  }
}

void *ev_apply_fans_load() {

  syslog(LOG_INFO, "ev_apply_fans_load() started.");

  const uint16_t fans_pin = 23;
  const size_t interval_sec = 1800;
  gpioSetMode(fans_pin, PI_OUTPUT);
  gpioSetPWMfrequency(fans_pin, 50); // Set GPIO23 to 50Hz.

  // Wait for all sensors to be read at least once
  sleep((pl.num_ext_sensors + pl.num_int_sensors) * 5);

  while (!ev_flag) {
    // Strictly speaking, there could be race conditions, but the consequence
    // should be fine for this particular purpose
    int _fans_load = (pl.int_temp == pl.ext_temp && pl.int_temp == 0)
                         ? 0
                         : ((pl.int_temp - pl.ext_temp) / 10 / 6);
    // i.e., (int_temp - ext_temp) > 6 degrees Celsius means 100% fans load
    _fans_load = _fans_load > 100 ? 100 : _fans_load;
    _fans_load = _fans_load < 0 ? 0 : _fans_load;
    pl.fans_load = _fans_load;

    if (gpioPWM(fans_pin, pl.fans_load / 100.0 * 254) != 0) {
      syslog(LOG_ERR, "Failed to set new fans load.");
    }
    save_data_to_db();
    for (size_t i = 0; i < interval_sec && !ev_flag; ++i) {
      sleep(1);
    }
  }
  syslog(LOG_INFO, "ev_apply_fans_load() quits gracefully.");
  return NULL;
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

void *ev_get_temp_from_sensors() {
  syslog(LOG_INFO, "ev_get_temp_from_sensors() started.");

  while (!ev_flag) {
    save_temp_to_payload(pl.int_sensors, pl.num_int_sensors, pl.int_temps,
                         &pl.int_temp);
    save_temp_to_payload(pl.ext_sensors, pl.num_ext_sensors, pl.ext_temps,
                         &pl.ext_temp);
  }
  syslog(LOG_INFO, "ev_get_temp_from_sensors() quits gracefully.");
  return NULL;
}

void *ev_set_7seg_display() {
  syslog(LOG_INFO, "ev_set_7seg_display() started.");
  init_7seg_display();
  uint8_t values[DIGIT_COUNT];
  bool dots[DIGIT_COUNT] = {0, 0, 1, 0, 0, 0, 1, 0};

  while (!ev_flag) {
    // We need to use an intermediary variable to avoid accessing pl members
    // multiple times; otherwise we can still trigger race condition

    const uint16_t fl = pl.fans_load * 10;
    const int _int_temp = pl.int_temp;

    values[0] = 10; // means turning the digit off
    values[1] = _int_temp % 100000 / 10000;
    values[2] = _int_temp % 10000 / 1000;
    values[3] = _int_temp % 1000 / 100;

    values[4] = fl % 10000 / 1000;
    if (values[4] == 0) {
      values[4] = 10; // means turning the digit off
    }
    values[5] = fl % 1000 / 100;
    values[6] = fl % 100 / 10;
    values[7] = fl % 10;
    show(values, dots);
  }
  char exit_msg[] = "ev_set_7seg_display() quits gracefully.\n";
  syslog(LOG_INFO, "%s", exit_msg);
  return NULL;
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

void init_payload() {
  for (size_t i = 0; i < pl.num_int_sensors; ++i) {
    pl.int_temps[i] = BAD_TEMPERATURE;
  }
  for (size_t i = 0; i < pl.num_ext_sensors; ++i) {
    pl.ext_temps[i] = BAD_TEMPERATURE;
  }
}

int main(int argc, char *argv[]) {
  int retval = 0;
  (void)openlog("rd", LOG_PID | LOG_CONS, 0);
  syslog(LOG_INFO, "rd started\n");
  pthread_t tids[5] = {0};
  parse_args(argc, argv);
  init_payload();
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

  if (pthread_create(&tids[0], NULL, ev_get_temp_from_sensors, NULL) != 0 ||
      pthread_create(&tids[1], NULL, ev_apply_fans_load, NULL) != 0 ||
      pthread_create(&tids[2], NULL, ev_monitor_rack_door, NULL) != 0 ||
      pthread_create(&tids[3], NULL, ev_set_7seg_display, NULL) != 0) {
    syslog(LOG_ERR, "Failed to create essential threads, program will quit");
    ev_flag = 1;
    retval = -1;
    goto err_pthread_create;
  }
  for (size_t i = 0; i < sizeof(tids) / sizeof(tids[0]); ++i) {
    pthread_join(tids[i], NULL);
  }

err_pthread_create:
  /* Stop DMA, release resources */
  gpioTerminate();
  syslog(LOG_INFO, "main() quits gracefully.");
err_install_signal_handler:
err_gpioInitialise:
err_prepare_database:
  closelog();
  return retval;
}
