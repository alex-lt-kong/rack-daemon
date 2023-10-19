#ifndef DATABASE_H
#define DATABASE_H

#include "global_vars.h"

#include <stdbool.h>

int prepare_database(const char *_db_path);
void save_data_to_db();
void save_rack_door_state_to_db(bool current_status);
ssize_t
get_top_six_door_states(int ids[],
                        char record_times[6][sizeof(SAMPLE_ISO_DT_STRING)],
                        int states[]);

#endif /* DATABASE_H */
