#
# $FreeBSD$
#

PROG=	mount_hpfs
SRCS=	mount_hpfs.c getmntopts.c

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${.CURDIR}/../../sys -I${MOUNT} -DHPFS
.PATH:	${MOUNT}

.include <bsd.prog.mk>
