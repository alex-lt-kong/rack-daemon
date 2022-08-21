main: ./src/rm.c
	gcc -o rm.out ./src/rm.c -lpigpio -lrt -lpthread -lsqlite3 -O2


clean:
	rm *.out