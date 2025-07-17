PACKAGE=runtime
PROG=	fdisk
SRCS=	fdisk.c fdisk_mbr_enc.c
WARNS?=	4
MAN=	fdisk.8

LIBADD=	geom

.include <bsd.prog.mk>

test:	${PROG}
	sh ${.CURDIR}/runtest.sh
