add_executable(rd main.c)

add_library(http_service http_service.c)

target_link_libraries(rd 
    http_service
    pigpio rt pthread sqlite3 microhttpd cjson
)
