runthis: STATS.o
	gcc -o runthis STATS.o

STATS.o: STATS.c shm.h
	gcc -c STATS.c