.PATH:	/usr/src/sbin/disklabel
OBJS=	tst01.o blocks.o disklabel.o dkcksum.o chunk.o disk.o change.o \
	create_chunk.o rules.o write_disk.o data.o
CFLAGS+= -Wall -g

all:	tst01

test:	tst01
	cp tst01 /0
	./tst01 wd1

fd:	tst01
	-umount /dev/fd1
	-umount /mnt
	mount /dev/fd1 /mnt
	strip tst01
	gzip < tst01 > /mnt/stand/disklayout
	chmod 755 /mnt/stand/disklayout
	-umount /mnt

BOOTS=/usr/mdec
data.o:
	file2c 'const unsigned char boot1[] = {' '};' \
                < ${BOOTS}/boot1 > tmp.c
	file2c 'const unsigned char boot2[] = {' '};' \
                < ${BOOTS}/boot2 >> tmp.c
	cc ${CFLAGS} -o data.o -c tmp.c

tst01:	${OBJS}
	#cc ${CFLAGS} -DREADLINE -static -o tst01 ${OBJS} -lreadline -ltermcap 
	cc ${CFLAGS} -static -o tst01 ${OBJS}

clean:
	rm -f *.o *.core tst01
