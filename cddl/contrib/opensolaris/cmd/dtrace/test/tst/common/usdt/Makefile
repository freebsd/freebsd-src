all: main

main: main.o prov.o
	$(CC) -o main main.o prov.o

main.o: main.c prov.h
	$(CC) -c main.c

prov.h: prov.d
	/usr/sbin/dtrace -h -s prov.d

prov.o: prov.d main.o
	/usr/sbin/dtrace -G -32 -s prov.d main.o
