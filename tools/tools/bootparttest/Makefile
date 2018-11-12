# $FreeBSD$

.PATH:		${.CURDIR}/../../../sys/boot/common

BINDIR?=	/usr/bin

PROG=		bootparttest
MAN=

SRCS=		bootparttest.c crc32.c stub.c part.c disk.c

CFLAGS=		-I${.CURDIR}/../../../sys/boot/common \
		-DLOADER_GPT_SUPPORT -DLOADER_MBR_SUPPORT -DPART_DEBUG \
		-DDISK_DEBUG

DPADD+=		${LIBGEOM}
LDADD+=		${LIBGEOM}
LDFLAGS+=	-lgeom

.include <bsd.prog.mk>
