add_executable(rd rd.c)

target_link_libraries(rd pigpio rt pthread sqlite3 curl opencv_videoio opencv_imgproc opencv_core opencv_imgcodecs)
