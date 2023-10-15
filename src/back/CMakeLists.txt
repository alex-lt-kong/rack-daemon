add_executable(rd main.c)

add_library(http_service_webapi http_service_webapi.c)
add_library(http_service_application http_service_application.c)
add_library(database database.c)
add_library(sensors sensors.c)
add_library(event_loops event_loops.c)
add_library(7seg_display 7seg_display.c)
add_library(utils utils.c)

target_link_libraries(rd 
    sensors http_service_webapi http_service_application database event_loops 7seg_display utils
    pigpio rt pthread sqlite3 microhttpd cjson
)
