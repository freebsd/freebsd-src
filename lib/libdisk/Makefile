# $FreeBSD$

LIB=	disk
SRCS=	blocks.c chunk.c disk.c change.c \
	create_chunk.c rules.c write_disk.c
INCS=	libdisk.h

CFLAGS+= 	-Wall
.if ${MACHINE} == "pc98"
CFLAGS+=	-DPC98
.endif
CLEANFILES+=	tmp.c tst01 tst01.o
NOPROFILE=	yes
NOPIC=		yes

MAN=		libdisk.3

.include <bsd.lib.mk>

tst01: tst01.o libdisk.a
	cc ${CFLAGS} -static tst01.o -o tst01 libdisk.a
