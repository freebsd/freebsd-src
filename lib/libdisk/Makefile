LIB=	disk
SRCS=	blocks.c disklabel.c dkcksum.c chunk.c disk.c change.c \
	create_chunk.c rules.c write_disk.c data.c

CFLAGS+= 	-Wall -g
CLEANFILES+=	tmp.c tst01 tst01.o data.c
VPATH=		${.CURDIR}/../../sbin/disklabel
NOPROFILE=	yes
NOSHARED=	yes
NOPIC=		yes

.include <bsd.lib.mk>

BOOTS=/usr/mdec

data.c:	libdisk.h ${BOOTS}/boot1 ${BOOTS}/boot2
	file2c 'const unsigned char boot1[] = {' '};' \
                < ${BOOTS}/boot1 > tmp.c
	file2c 'const unsigned char boot2[] = {' '};' \
                < ${BOOTS}/boot2 >> tmp.c
	mv tmp.c data.c

tst01: tst01.o libdisk.a
	cc ${CFLAGS} -static tst01.o -o tst01 libdisk.a
