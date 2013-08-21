#	@(#)Makefile	8.6 (Berkeley) 5/8/95
# $FreeBSD$

PROG=	mount
SRCS=	mount.c mount_fs.c getmntopts.c vfslist.c
MAN=	mount.8 mount.conf.8
# We do NOT install the getmntopts.3 man page.

DPADD=	${LIBUTIL}
LDADD=	-lutil

.include <bsd.prog.mk>
