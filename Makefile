main: rm.out

rm.out: ./src/back/rm.c cam.so
	gcc -o rm.out ./src/back/rm.c -lpigpio -lrt -lpthread -lsqlite3 -l:cam.so -Wall -O2
cam.so: ./src/back/cam.cpp ./src/back/cam.h
	g++ -shared -o /usr/local/lib/cam.so ./src/back/cam.cpp -lopencv_videoio -lopencv_imgproc -lopencv_core -O2
clean:
	rm *.out /usr/local/lib/cam.so