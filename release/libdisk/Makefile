.PATH:	/usr/src/sbin/disklabel
OBJS=	tst01.o blocks.o disklabel.o dkcksum.o chunk.o disk.o change.o \
	create_chunk.o
CFLAGS+= -Wall

test:	tst01
	cp tst01 /0
	./tst01 sd0 

tst01:	${OBJS}
	cc ${CFLAGS} -static -o tst01 ${OBJS}
