#include <curl/curl.h>
#include <libgen.h>
#include <limits.h>
#include <pigpio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <sqlite3.h>
#include <stdio.h>
#include <syslog.h>
#include <unistd.h>
#include <linux/limits.h>
#include <signal.h>

#include "7seg.c"


struct Payload {
    int32_t temps[4];
    float fans_load;
};

volatile sig_atomic_t done = 0;
char db_path[PATH_MAX];

void signal_handler(int signum) {
    done = 1;
    char msg[] = "Signal [  ] caught\n";
    msg[8] = '0' + signum / 10;
    msg[9] = '0' + signum % 10;
    write(STDERR_FILENO, msg, strlen(msg));
}

void* thread_monitor_main_entrance() {
    syslog(LOG_INFO, "thread_monitor_main_entrance() started.");
    char envvar[] = "RD_TELEMETRY_ENDPOINT";
    if(!getenv(envvar)) {
        syslog(LOG_INFO, "The environment variable [%s] was not found, "
            "thread_report_sensor_readings() quits gracefully.", envvar);
        return NULL;
    }
    char telemetry_endpoint[PATH_MAX];
    char url[PATH_MAX];
    if(snprintf(telemetry_endpoint, PATH_MAX, "%s", getenv(envvar)) >= PATH_MAX){
        syslog(LOG_INFO, "PATH_MAX too small for %s, "
            "thread_report_sensor_readings() quits gracefully.", envvar);
        return NULL;
    }
    const int pin_pos = 26;
    const int pin_neg = 20;


    gpioSetMode(pin_pos, PI_OUTPUT);
    gpioSetMode(pin_neg, PI_INPUT);
    gpioWrite(pin_pos, PI_HIGH);
    gpioSetPullUpDown(pin_neg, PI_PUD_UP);

    int open_status_count = 0;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    while (!done) {
        if (gpioRead(pin_neg) == 0) {
            ++ open_status_count;
            if (open_status_count == 5) {
                CURL *curl;
                CURLcode res;
                curl = curl_easy_init();
                if(curl) {
                    snprintf(url, PATH_MAX, telemetry_endpoint, 0);
                    curl_easy_setopt(curl, CURLOPT_URL, url);
                    /* Perform the request, res will get the return code */
                    res = curl_easy_perform(curl);
                    /* Check for errors */
                    if(res != CURLE_OK)
                        syslog(LOG_ERR, "curl_easy_perform() failed: %s\n",
                            curl_easy_strerror(res));

                    /* always cleanup */
                    curl_easy_cleanup(curl);
                } else {
                    syslog(LOG_ERR, "Failed to create curl instance by curl_easy_init().");
                }
            }
        } else {
            open_status_count = 0;
        }
        sleep(1);
    }
    curl_global_cleanup();
    char exit_msg[] = "thread_monitor_main_entrance() quits gracefully\n";
    syslog(LOG_INFO, exit_msg);
    printf(exit_msg);
    return NULL;
}

void* thread_monitor_rack_door() {
    syslog(LOG_INFO, "thread_monitor_rack_door() started.");
    const int pin_pos = 19;
    const int pin_neg = 16;

    const char* sql_create =
        "CREATE TABLE IF NOT EXISTS door_state"
        "("
        "  [record_id] INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  [record_time] TEXT,"
        "  [door_state] INTEGER"
        ")";
    const char* sql_insert = "INSERT INTO door_state"
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
    while (!done) {
        usleep(1000 * 1000); // i.e., 1sec
        current_status = gpioRead(pin_neg);
        if (current_status != last_status) {
            ++ status_count;
            if (status_count < 5) {
                continue;
            }
            last_status = current_status;

            int rc = sqlite3_open(db_path, &db);
            if (rc != SQLITE_OK) {
                syslog(LOG_ERR, "Cannot open database [%s]: %s. INSERT will be "
                "skipped\n", db_path, sqlite3_errmsg(db));
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
            if(stmt != NULL) {
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
                    syslog(LOG_ERR,
                        "SQL error: %d. INSERT is not successful.\n", rc);
                    sqlite3_close(db);
                    continue;
                }
            }
            sqlite3_close(db);
        } else {
            status_count = 0;
        }
    }

    char exit_msg[] = "thread_monitor_rack_door() quits gracefully.\n";
    syslog(LOG_INFO, exit_msg);
    printf(exit_msg);
    return NULL;
}

void* thread_apply_fans_load(void* payload) {

    syslog(LOG_INFO, "thread_apply_fans_load() started.");
    struct Payload* pl = (struct Payload*)payload;
    const char* sql_create = "CREATE TABLE IF NOT EXISTS temp_control"
                "("
                "  [record_id] INTEGER PRIMARY KEY AUTOINCREMENT,"
                "  [record_time] TEXT,"
                "  [external_temp_0] INTEGER,"
                "  [external_temp_1] INTEGER,"
                "  [internal_temp_0] INTEGER,"
                "  [internal_temp_1] INTEGER,"
                "  [fans_load] REAL"
                ")";
    const char* sql_insert = "INSERT INTO temp_control"
            "(record_time, external_temp_0, external_temp_1, "
            "internal_temp_0, internal_temp_1, fans_load) "
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

    while (!done) {
        sleep(1);
        iter += 1;
        int16_t delta = ((pl->temps[0] + pl->temps[1]) - (pl->temps[2] + pl->temps[3])) / 2;
        pl->fans_load = delta / 1000.0 / 8.0;
        // i.e., delta temp > 8 means 100%
        pl->fans_load = pl->fans_load > 1.0 ? 1.0 : pl->fans_load;
        pl->fans_load = pl->fans_load < 0.0 ? 0 : pl->fans_load;
        //printf("fans_load_regulated: %f\n", fans_load);
        if (gpioPWM(fans_pin, pl->fans_load * 254) != 0) {
            syslog(LOG_ERR, "Failed to set new fans load.");
            continue;
        }
        if (iter < interval) {
            continue;
        }
        iter = 0;


        int rc = sqlite3_open(db_path, &db);
        if (rc != SQLITE_OK) {
            syslog(LOG_ERR, "Cannot open database [%s]: %s. INSERT will be "
                "skipped\n", db_path, sqlite3_errmsg(db));
            sqlite3_close(db);
            continue;
        }
        rc = sqlite3_exec(db, sql_create, 0, 0, &sqlite_err_msg);
        if (rc != SQLITE_OK) {
            syslog(LOG_ERR, "SQL error: %s. CREATE is not successful.\n", sqlite_err_msg);
            sqlite3_free(sqlite_err_msg);
            sqlite3_close(db);
            continue;
        }
        sqlite3_stmt *stmt;
        sqlite3_prepare_v2(db, sql_insert, 512, &stmt, NULL);
        if(stmt != NULL) {
            time(&now);
            char buf[sizeof("1970-01-01 00:00:00")];
            strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", localtime(&now));
            sqlite3_bind_text(stmt, 1, buf, -1, NULL);
            sqlite3_bind_int(stmt, 2, pl->temps[0]);
            sqlite3_bind_int(stmt, 3, pl->temps[1]);
            sqlite3_bind_int(stmt, 4, pl->temps[2]);
            sqlite3_bind_int(stmt, 5, pl->temps[3]);;
            sqlite3_bind_double(stmt, 6, pl->fans_load);
            sqlite3_step(stmt);
            rc = sqlite3_finalize(stmt);
            if (rc != SQLITE_OK) {
                syslog(LOG_ERR, "SQL error: %d. INSERT is not successful.\n", rc);
                sqlite3_close(db);
                continue;
            }
        }
        sqlite3_close(db);
    }
    char exit_msg[] = "thread_apply_fans_load() quits gracefully.\n";
    syslog(LOG_INFO, exit_msg);
    printf(exit_msg);
    return NULL;
}

void* thread_get_readings_from_sensors(void* payload) {
    syslog(LOG_INFO, "thread_get_readings_from_sensors() started.");
    struct Payload* pl = (struct Payload*)payload;

    char sensors[4][256] = {
        "/sys/bus/w1/devices/28-0301a279faf2/w1_slave",
        "/sys/bus/w1/devices/28-030997792b61/w1_slave",
        "/sys/bus/w1/devices/28-01144ebe52aa/w1_slave",
        "/sys/bus/w1/devices/28-01144ef1faaa/w1_slave"
    };
    char buf[256];
    int fd;

    while (!done) {
        for (int i = 0; i < 4 && !done; ++i) {
            // takes around 1 sec to read value from one sensor.
            fd = open(sensors[i], O_RDONLY);
            if(fd >= 0) {
                if(read( fd, buf, sizeof(buf) ) > 0) {
                    char* temp_str = strstr(buf, "t=") + 2;
                    sscanf(temp_str, "%d", &(pl->temps[i]));
                }
                close(fd);
            } else {
                syslog(LOG_ERR, "Unable to open device at [%s], skipped this "
                    "read attempt.", sensors[i]);
            }
            sleep(1);
        }
    }
    char exit_msg[] = "thread_get_readings_from_sensors() quits gracefully.\n";
    syslog(LOG_INFO, exit_msg);
    printf(exit_msg);
    return NULL;
}

void* thread_set_7seg_display(void* payload) {
    syslog(LOG_INFO, "thread_set_7seg_display() started.");
    struct Payload* pl = (struct Payload*)payload;
    init_7seg_display();
    uint8_t values[DIGIT_COUNT];
    bool dots[DIGIT_COUNT] = {0,0,1,0,0,0,1,0};
    int32_t internal_temp;

    while (!done) {
        internal_temp = (pl->temps[0] + pl->temps[1]) / 2;
        values[0] = 10; // means turning the digit off
        values[1] = internal_temp % 100000 / 10000;
        values[2] = internal_temp % 10000 / 1000;
        values[3] = internal_temp % 1000 / 100;

        uint16_t fl = pl->fans_load * 1000;
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
    syslog(LOG_INFO, exit_msg);
    printf(exit_msg);
    return NULL;
}

int main(void) {
    openlog("rd.out", LOG_PID | LOG_CONS, 0);
    syslog(LOG_INFO, "rd.out started\n");
    pthread_t tids[5];

    if (gpioInitialise() < 0) {
        syslog(LOG_ERR, "pigpio initialisation failed, program will quit\n");
        closelog();
        return 1;
    }
    struct sigaction act;
    act.sa_handler = signal_handler;
    act.sa_flags = 0;
    if (sigemptyset(&act.sa_mask) == -1) {
        perror("sigemptyset()");
        return EXIT_FAILURE;
    }
    if (sigaction(SIGINT,  &act, 0) == -1 ||
        sigaction(SIGABRT, &act, 0) == -1 ||
        sigaction(SIGTERM, &act, 0) == -1) {
        perror("sigaction()");
        return EXIT_FAILURE;
    }
    snprintf(db_path, PATH_MAX, "%s", getenv("RD_DB_DIR"));
    struct Payload pl;
    pl.temps[0] = 65535;
    pl.temps[1] = 65535;
    pl.temps[2] = 65535;
    pl.temps[3] = 65535;
    if (
        pthread_create(&tids[0], NULL, thread_get_readings_from_sensors, &pl) != 0 ||
        pthread_create(&tids[1], NULL, thread_apply_fans_load, &pl) != 0 ||
        pthread_create(&tids[2], NULL, thread_monitor_rack_door, NULL) != 0 ||
        pthread_create(&tids[3], NULL, thread_set_7seg_display, &pl) != 0 ||
        pthread_create(&tids[4], NULL, thread_monitor_main_entrance, NULL) != 0

    ) {
        syslog(LOG_ERR, "Failed to create essential threads, program will quit\n");
        done = 1;
        closelog();
        gpioTerminate();
        return 1;
    }
    for (size_t i = 0; i < sizeof(tids) / sizeof(tids[0]); ++i) {
        pthread_join(tids[i], NULL);
    }
    /* Stop DMA, release resources */
    gpioTerminate();
    const char exit_msg[] = "main() quits gracefully.\n";
    syslog(LOG_INFO, exit_msg);
    printf(exit_msg);
    closelog();
    return 0;
}
