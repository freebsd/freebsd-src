#
#	$Id: Makefile,v 1.6 1994/07/16 21:32:06 cgd Exp $
#

PROG=	mount_msdos
SRCS=	mount_msdos.c getmntopts.c
MAN8=	mount_msdos.8

BINOWN=	root
BINMODE= 4555

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
