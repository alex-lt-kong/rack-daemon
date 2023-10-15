#ifndef DATABASE_H
#define DATABASE_H

#include <stdbool.h>

int prepare_database();
void save_data_to_db();
void save_rack_door_state_to_db(bool current_status);

#endif /* DATABASE_H */
