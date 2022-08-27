main: ./src/rm.c ./src/v4l.c
	gcc -o rm.out ./src/rm.c -lpigpio -lrt -lpthread -lsqlite3 -O2
	gcc -o ocv.out ./src/ocv.c -lopencv_dnn -lopencv_highgui -lopencv_ml -lopencv_objdetect -lopencv_shape -lopencv_stitching -lopencv_superres -lopencv_videostab -lopencv_calib3d -lopencv_videoio -lopencv_imgcodecs -lopencv_features2d -lopencv_video -lopencv_photo -lopencv_imgproc -lopencv_flann -lopencv_core -O2

clean:
	rm *.out