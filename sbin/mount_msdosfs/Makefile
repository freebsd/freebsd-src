#
#	$Id: Makefile,v 1.4 1997/02/22 14:32:29 peter Exp $
#

PROG=	mount_msdos
SRCS=	mount_msdos.c getmntopts.c
MAN8=	mount_msdos.8

BINOWN=	root
BINMODE= 4555

MOUNT=	${.CURDIR}/../../mount
CFLAGS+= -D_NEW_VFSCONF
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
