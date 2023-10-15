#include "database.h"
#include "utils.h"

#include <linux/limits.h>
#include <sqlite3.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syslog.h>
#include <time.h>

char db_path[PATH_MAX];

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

void save_data_to_db() {
  const char *sql_insert =
      "INSERT INTO temp_control"
      "(record_time, external_temps, internal_temps, fans_load) "
      "VALUES(?, ?, ?, ?);";
  time_t now;
  int res;
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
  if ((res = sqlite3_step(stmt)) != SQLITE_OK) {
    syslog(LOG_ERR, "sqlite3_step() failed: %d(%s). INSERT skipped", res,
           sqlite3_errmsg(db));
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

void save_rack_door_state_to_db(bool current_status) {
  int res;
  sqlite3 *db;
  time_t now;
  const char sql_insert[] = "INSERT INTO door_state"
                            "(record_time, door_state) "
                            "VALUES(?, ?);";
  int rc = sqlite3_open(db_path, &db);
  if (rc != SQLITE_OK) {
    syslog(LOG_ERR, "Cannot open database [%s]: %s. INSERT skipped", db_path,
           sqlite3_errmsg(db));
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
  if ((res = sqlite3_step(stmt)) != SQLITE_OK) {
    syslog(LOG_ERR, "sqlite3_step() failed: %d(%s). INSERT skipped", res,
           sqlite3_errmsg(db));
  }
err_sqlite3_bind:
  rc = sqlite3_finalize(stmt);
  if (rc != SQLITE_OK) {
    syslog(LOG_ERR, "SQL error: %d. INSERT is not successful.\n", rc);
    sqlite3_close(db);
  }
err_sqlite3_prepare:
err_sqlite3_open:
  sqlite3_close(db);
}
