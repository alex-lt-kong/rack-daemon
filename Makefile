OPT = -O3 -Wall

main: rd.out

rd.out: ./src/back/rd.c cam.so
	gcc -o rd.out ./src/back/rd.c -lpigpio -lrt -lpthread -lsqlite3 -lcurl -l:cam.so $(OPT)
cam.so: ./src/back/cam.cpp ./src/back/cam.h
	g++ -shared -o /usr/local/lib/cam.so ./src/back/cam.cpp -lopencv_videoio -lopencv_imgproc -lopencv_core $(OPT)
clean:
	rm *.out /usr/local/lib/cam.so