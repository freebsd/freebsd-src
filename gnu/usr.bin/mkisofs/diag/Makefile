#CC = gcc
CC = cc -traditional

all: dump isodump isovfy 

isoinfo:isoinfo.c
	\${CC} -g -o isoinfo isoinfo.c
dump:dump.c
	\${CC} -o dump dump.c
isodump:isodump.c
	\${CC} -o isodump isodump.c
isovfy:isovfy.c
	\${CC} -o isovfy isovfy.c

clean:
	rm -f dump isodump isovfy isoinfo
