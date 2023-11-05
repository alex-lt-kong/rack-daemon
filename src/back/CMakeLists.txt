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