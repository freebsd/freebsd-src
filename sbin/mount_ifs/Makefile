#	@(#)Makefile	8.6 (Berkeley) 5/8/95
#	$Id$

PROG=	mount
SRCS=	mount.c mount_ufs.c getmntopts.c vfslist.c
MAN8=	mount.8
# We do NOT install the getmntopts.3 man page.
CFLAGS+=	-D_NEW_VFSCONF
NOSHARED=	yes

.include <bsd.prog.mk>
