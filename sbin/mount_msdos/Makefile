#
#	$Id: Makefile,v 1.6 1997/10/21 07:26:50 bde Exp $
#

PROG=	mount_msdos
SRCS=	mount_msdos.c getmntopts.c
MAN8=	mount_msdos.8

MOUNT=	${.CURDIR}/../../mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
