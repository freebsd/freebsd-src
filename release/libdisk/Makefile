LIB=	disk
SRCS=	blocks.c disklabel.c dkcksum.c chunk.c disk.c change.c \
	create_chunk.c rules.c write_disk.c data.c

CFLAGS+= 	-Wall -g
CLEANFILES+=	tmp.c tst01 tst01.o
VPATH=		${.CURDIR}/../../sbin/disklabel
NOPROFILE=	yes
NOSHARED=	yes

.include <bsd.lib.mk>

# Custom weird and funky targets that we'll leave here.
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
data.c:
	file2c 'const unsigned char boot1[] = {' '};' \
                < ${BOOTS}/boot1 > tmp.c
	file2c 'const unsigned char boot2[] = {' '};' \
                < ${BOOTS}/boot2 >> tmp.c
	mv tmp.c data.c

tst01: tst01.o
	cc ${CFLAGS} -static tst01.o -o tst01 -L${.CURDIR} -ldisk
