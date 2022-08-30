main: rm.out

rm.out: ./src/rm.c cam.so
	gcc -o rm.out ./src/rm.c -lpigpio -lrt -lpthread -lsqlite3 -l:cam.so -Wall -O2
cam.so: ./src/cam.cpp ./src/cam.h
	g++ -shared -o /usr/local/lib/cam.so ./src/cam.cpp -lopencv_videoio -lopencv_imgproc -lopencv_core -O2
clean:
	rm *.out /usr/local/lib/cam.so