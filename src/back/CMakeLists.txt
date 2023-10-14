add_subdirectory(http_service/)

include_directories(http_service/)

add_executable(rd main.c)

target_link_libraries(rd pigpio rt pthread sqlite3 httpService cjson)
