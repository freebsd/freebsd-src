# $FreeBSD$

LIB=	disk
SRCS=	blocks.c chunk.c disk.c change.c \
	create_chunk.c rules.c write_disk.c write_${MACHINE}_disk.c
INCS=	libdisk.h

WARNS=	2

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

ad0:	all install tst01
	./tst01 ad0

da0:	all install tst01
	./tst01 da0

da1:	all install tst01
	./tst01 da1
