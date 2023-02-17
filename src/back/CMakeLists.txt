add_executable(rd rd.c)
add_library(cam cam.cpp)

target_link_libraries(cam PUBLIC opencv_videoio opencv_imgproc opencv_core)
target_link_libraries(rd cam pigpio rt pthread sqlite3 curl opencv_videoio opencv_imgproc opencv_core opencv_imgcodecs)
