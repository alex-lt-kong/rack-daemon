add_executable(rd main.c)

target_link_libraries(rd pigpio rt pthread sqlite3 curl)
