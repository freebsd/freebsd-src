#	$FreeBSD$

PROG=	mount_nwfs
SRCS=	mount_nwfs.c getmntopts.c
MAN8=	mount_nwfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -DNWFS -I${MOUNT}

.PATH:	${MOUNT}

LDADD+= -lncp -lipx
DPADD+= ${LIBIPX} ${LIBNCP}

.include <bsd.prog.mk>
