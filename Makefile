main: rm.out

rm.out: ./src/rm.c cam.so
	gcc -L. -o rm.out ./src/rm.c -lpigpio -lrt -lpthread -lsqlite3 -l:cam.so -Wall  -O2
cam.so: ./src/cam.cpp ./src/cam.h
	g++ -shared -o cam.so ./src/cam.cpp -lopencv_videoio -lopencv_imgcodecs -lopencv_video -lopencv_photo -lopencv_imgproc -lopencv_core -O2
#cam.so: cam.o#
	#g++ -shared -o cam.so cam.o
clean:
	rm *.out *.o *.so