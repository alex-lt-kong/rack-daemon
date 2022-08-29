main: ./src/rm.c
	gcc -o rm.out ./src/rm.c -lpigpio -lrt -lpthread -lsqlite3 -lopencv_videoio -lopencv_imgcodecs -lopencv_video -lopencv_photo -lopencv_imgproc -lopencv_flann -lopencv_core -O2

clean:
	rm *.out