#
#	$FreeBSD$
#

PROG=	mount_msdos
SRCS=	mount_msdos.c getmntopts.c
MAN8=	mount_msdos.8

BINOWN=	root
BINMODE= 4555

MOUNT=	${.CURDIR}/../../mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
