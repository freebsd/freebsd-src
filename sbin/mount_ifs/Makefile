#	@(#)Makefile	8.6 (Berkeley) 5/8/95
#	$Id: Makefile,v 1.6 1998/01/20 10:39:57 bde Exp $

PROG=	mount
SRCS=	mount.c mount_ufs.c getmntopts.c vfslist.c
MAN8=	mount.8
# We do NOT install the getmntopts.3 man page.

# We support the ROOTSLICE_HUNT hack
CFLAGS+=-DROOTSLICE_HUNT

.include <bsd.prog.mk>
