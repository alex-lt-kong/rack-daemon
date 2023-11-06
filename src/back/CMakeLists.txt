find_library(SQLITE3_LIB sqlite3)
if(NOT SQLITE3_LIB)
  message(FATAL_ERROR "sqlite3 library not found, install it with 'apt install libsqlite3-dev'")
endif()
find_package(cJSON REQUIRED)

add_executable(rd
    main.c
    http_service_webapi.c
    http_service_application.c
    database.c
    sensors.c
    event_loops.c
    utils.c
)

target_link_libraries(rd 
    iotctrl
    gpiod modbus pigpio rt pthread sqlite3 microhttpd cjson
)