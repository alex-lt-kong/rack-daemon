#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <linux/limits.h>
#include <pigpio.h>
#include <signal.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syslog.h>
#include <syslog.h>
#include <unistd.h>

#include <curl/curl.h>

#include "7seg.c"

#define BAD_TEMPERATURE 65535

struct Payload {
  int32_t hi_temps[2];
  int32_t lo_temps[2];
  float fans_load;
};

volatile sig_atomic_t ev_flag = 0;
char db_path[PATH_MAX];

void signal_handler(int signum) {
  ev_flag = 1;
  char msg[] = "Signal [  ] caught\n";
  msg[8] = '0' + signum / 10;
  msg[9] = '0' + signum % 10;
  write(STDERR_FILENO, msg, strlen(msg));
}

void *ev_monitor_rack_door() {
  syslog(LOG_INFO, "ev_monitor_rack_door() started.");
  const int pin_pos = 19;
  const int pin_neg = 16;

  const char sql_create[] = "CREATE TABLE IF NOT EXISTS door_state"
                            "("
                            "  [record_id] INTEGER PRIMARY KEY AUTOINCREMENT,"
                            "  [record_time] TEXT,"
                            "  [door_state] INTEGER"
                            ")";
  const char sql_insert[] = "INSERT INTO door_state"
                            "(record_time, door_state) "
                            "VALUES(?, ?);";
  char *sqlite_err_msg = 0;
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
        syslog(LOG_ERR,
               "Cannot open database [%s]: %s. INSERT will be "
               "skipped\n",
               db_path, sqlite3_errmsg(db));
        sqlite3_close(db);
        continue;
      }
      rc = sqlite3_exec(db, sql_create, 0, 0, &sqlite_err_msg);
      if (rc != SQLITE_OK) {
        syslog(LOG_ERR, "SQL error: %s. CREATE is not successful.\n",
               sqlite_err_msg);
        sqlite3_free(sqlite_err_msg);
        sqlite3_close(db);
        continue;
      }
      sqlite3_stmt *stmt;
      sqlite3_prepare_v2(db, sql_insert, 512, &stmt, NULL);
      if (stmt != NULL) {
        time(&now);
        char buf[sizeof("1970-01-01 00:00:00")];
        strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", localtime(&now));
        sqlite3_bind_text(stmt, 1, buf, -1, NULL);
        sqlite3_bind_int(stmt, 2, !current_status);
        /* here we try to be consistent with common sense:
          1 means "triggered" and thus "opened"*/
        sqlite3_step(stmt);
        rc = sqlite3_finalize(stmt);
        if (rc != SQLITE_OK) {
          syslog(LOG_ERR, "SQL error: %d. INSERT is not successful.\n", rc);
          sqlite3_close(db);
          continue;
        }
      }
      sqlite3_close(db);
    } else {
      status_count = 0;
    }
  }

  syslog(LOG_INFO, "ev_monitor_rack_door() quits gracefully.");
  return NULL;
}

void *ev_apply_fans_load(void *payload) {

  syslog(LOG_INFO, "ev_apply_fans_load() started.");
  struct Payload *pl = (struct Payload *)payload;
  const char *sql_create = "CREATE TABLE IF NOT EXISTS temp_control"
                           "("
                           "  [record_id] INTEGER PRIMARY KEY AUTOINCREMENT,"
                           "  [record_time] TEXT,"
                           "  [external_temp_0] INTEGER,"
                           "  [external_temp_1] INTEGER,"
                           "  [internal_temp_0] INTEGER,"
                           "  [internal_temp_1] INTEGER,"
                           "  [fans_load] REAL"
                           ")";
  const char *sql_insert = "INSERT INTO temp_control"
                           "(record_time, internal_temp_0, internal_temp_1, "
                           "external_temp_0, external_temp_1, fans_load) "
                           "VALUES(?, ?, ?, ?, ?, ?);";
  char *sqlite_err_msg = 0;
  time_t now;

  const uint16_t fans_pin = 23;
  gpioSetMode(fans_pin, PI_OUTPUT);
  gpioSetPWMfrequency(fans_pin, 50); // Set GPIO23 to 50Hz.

  size_t interval = 3600;
  size_t iter = interval - 60;
  // we wait for 10 sec before first DB writing--
  // so that we wont write non-sense default values to DB
  sqlite3 *db;

  while (!ev_flag) {
    sleep(1);
    iter += 1;
    int hi_temp = 0, lo_temp = 0;
    for (size_t i = 0; i < sizeof(pl->hi_temps) / sizeof(pl->hi_temps[0]);
         ++i) {
      if (pl->hi_temps[i] != BAD_TEMPERATURE) {
        hi_temp += pl->hi_temps[i];
      }
    }
    for (size_t i = 0; i < sizeof(pl->lo_temps) / sizeof(pl->lo_temps[0]);
         ++i) {
      if (pl->lo_temps[i] != BAD_TEMPERATURE) {
        lo_temp += pl->lo_temps[i];
      }
    }
    int16_t delta = (hi_temp - lo_temp) / 2;
    pl->fans_load = delta / 1000.0 / 6.0;
    // i.e., delta temp > 6 means 100%
    pl->fans_load = pl->fans_load > 1.0 ? 1.0 : pl->fans_load;
    pl->fans_load = pl->fans_load < 0.0 ? 0 : pl->fans_load;
    // printf("fans_load_regulated: %f\n", fans_load);
    if (gpioPWM(fans_pin, pl->fans_load * 254) != 0) {
      syslog(LOG_ERR, "Failed to set new fans load.");
      goto err_pwm_failure;
    }
    if (iter < interval) {
      continue;
    }
    iter = 0;

    if (sqlite3_open(db_path, &db) != SQLITE_OK) {
      syslog(LOG_ERR, "Cannot open database [%s]: %s. INSERT will be skipped",
             db_path, sqlite3_errmsg(db));
      goto err_sqlite3_open;
    }
    if (sqlite3_exec(db, sql_create, 0, 0, &sqlite_err_msg) != SQLITE_OK) {
      syslog(LOG_ERR, "SQL error: %s. CREATE is not successful.\n",
             sqlite_err_msg);
      (void)sqlite3_free(sqlite_err_msg);
      goto err_sqlite3_create;
    }
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql_insert, 512, &stmt, NULL) != SQLITE_OK) {
      syslog(LOG_ERR, "sqlite3_prepare_v2() error, INSERT will be skipped");
      goto err_sqlite3_prepare;
    }
    // stmt should not be NULL if sqlite3_prepare_v2() returns SQLITE_OK

    time(&now);
    char buf[sizeof("1970-01-01 00:00:00")];
    strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", localtime(&now));
    if (sqlite3_bind_text(stmt, 1, buf, -1, NULL) != SQLITE_OK ||
        sqlite3_bind_int(stmt, 2, pl->hi_temps[0]) != SQLITE_OK ||
        sqlite3_bind_int(stmt, 3, pl->hi_temps[1]) != SQLITE_OK ||
        sqlite3_bind_int(stmt, 4, pl->lo_temps[0]) != SQLITE_OK ||
        sqlite3_bind_int(stmt, 5, pl->lo_temps[1]) != SQLITE_OK ||
        sqlite3_bind_double(stmt, 6, pl->fans_load) != SQLITE_OK) {
      syslog(LOG_ERR, "sqlite3_bind_...() failed. INSERT is not successful.");
      goto err_sqlite_bind;
    }

    sqlite3_step(stmt);
    if (sqlite3_finalize(stmt) != SQLITE_OK) {
      syslog(LOG_ERR, "sqlite3_finalize() failed. INSERT is not successful.");
    }
  err_sqlite_bind:
  err_sqlite3_prepare:
  err_sqlite3_finalize:
  err_sqlite3_create:
  err_sqlite3_open:
    // Whether or not an error occurs when it is opened, resources associated
    // with the database connection handle should be released by passing it to
    // sqlite3_close() when it is no longer required.
    if (sqlite3_close(db) != SQLITE_OK) {
      syslog(LOG_ERR, "sqlite3_close() failed.");
    }
  err_pwm_failure:
    continue;
  }
  syslog(LOG_INFO, "ev_apply_fans_load() quits gracefully.");
  return NULL;
}

void save_temp_to_payload(char sensors[][PATH_MAX], const size_t sensor_count,
                          int32_t *temps) {
  char buf[PATH_MAX];
  int fd;
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
    } else {
      syslog(LOG_ERR,
             "Unable to open device at [%s], errno: %d(%s), skipped this "
             "read attempt.",
             sensors[i], errno, strerror(errno));
    }
    sleep(1);
  }
}

void *ev_get_temp_from_sensors(void *payload) {
  syslog(LOG_INFO, "ev_get_temp_from_sensors() started.");
  struct Payload *pl = (struct Payload *)payload;
  char int_sensors[][PATH_MAX] = {
      "/sys/bus/w1/devices/28-0301a279faf2/w1_slave",
      "/sys/bus/w1/devices/28-030997792b61/w1_slave",
  };

  char ext_sensors[][PATH_MAX] = {
      "/sys/bus/w1/devices/28-01144ebe52aa/w1_slave",
      "/sys/bus/w1/devices/28-01144ef1faaa/w1_slave"};

  while (!ev_flag) {
    save_temp_to_payload(int_sensors,
                         sizeof(int_sensors) / sizeof(int_sensors[0]),
                         pl->hi_temps);
    save_temp_to_payload(ext_sensors,
                         sizeof(ext_sensors) / sizeof(ext_sensors[0]),
                         pl->lo_temps);
  }
  syslog(LOG_INFO, "ev_get_temp_from_sensors() quits gracefully.");
  return NULL;
}

void *thread_set_7seg_display(void *payload) {
  syslog(LOG_INFO, "thread_set_7seg_display() started.");
  struct Payload *pl = (struct Payload *)payload;
  init_7seg_display();
  uint8_t values[DIGIT_COUNT];
  bool dots[DIGIT_COUNT] = {0, 0, 1, 0, 0, 0, 1, 0};
  int32_t internal_temp;

  while (!ev_flag) {

    internal_temp = (pl->hi_temps[0] + pl->hi_temps[1]) / 2;
    uint16_t fl = pl->fans_load * 1000;

    values[0] = 10; // means turning the digit off
    values[1] = internal_temp % 100000 / 10000;
    values[2] = internal_temp % 10000 / 1000;
    values[3] = internal_temp % 1000 / 100;

    values[4] = fl % 10000 / 1000;
    if (values[4] == 0) {
      values[4] = 10; // means turning the digit off
    }
    values[5] = fl % 1000 / 100;
    values[6] = fl % 100 / 10;
    values[7] = fl % 10;
    show(values, dots);
  }
  char exit_msg[] = "thread_set_7seg_display() quits gracefully.\n";
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

int main(void) {
  int retval = 0;
  (void)openlog("rd", LOG_PID | LOG_CONS, 0);
  syslog(LOG_INFO, "rd started\n");
  pthread_t tids[5];

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

  snprintf(db_path, PATH_MAX, "%s", getenv("RD_DB_DIR"));
  struct Payload pl;
  pl.hi_temps[0] = BAD_TEMPERATURE;
  pl.hi_temps[1] = BAD_TEMPERATURE;
  pl.lo_temps[0] = BAD_TEMPERATURE;
  pl.lo_temps[1] = BAD_TEMPERATURE;
  if (pthread_create(&tids[0], NULL, ev_get_temp_from_sensors, &pl) != 0 ||
      pthread_create(&tids[1], NULL, ev_apply_fans_load, &pl) != 0 ||
      pthread_create(&tids[2], NULL, ev_monitor_rack_door, NULL) != 0 ||
      pthread_create(&tids[3], NULL, thread_set_7seg_display, &pl) != 0) {
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
  closelog();
  return retval;
}
