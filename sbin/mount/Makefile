#	@(#)Makefile	8.5 (Berkeley) 3/27/94

PROG=	mount
SRCS=	mount.c mount_ufs.c getmntopts.c
MAN8=	mount.8
# We do NOT install the getmntopts.3 man page.

# We support the ROOTSLICE_HUNT hack
CFLAGS+=-DROOTSLICE_HUNT

.include <bsd.prog.mk>
