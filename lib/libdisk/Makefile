# $FreeBSD$

LIB=	disk
SRCS=	blocks.c chunk.c disk.c change.c \
	create_chunk.c rules.c write_disk.c
INCS=	libdisk.h

WARNS=	2

.if ${MACHINE} == "pc98"
CFLAGS+=	-DPC98
SRCS	+=	write_pc98_disk.c
.endif

.if ${MACHINE} == "i386"
SRCS	+=	write_i386_disk.c
.endif

.if ${MACHINE} == "alpha"
SRCS	+=	write_alpha_disk.c
.endif

CLEANFILES+=	tmp.c tst01 tst01.o
NOPROFILE=	yes
NOPIC=		yes

MAN=		libdisk.3

.include <bsd.lib.mk>

tst01: tst01.o libdisk.a
	cc ${CFLAGS} -static tst01.o -o tst01 libdisk.a
