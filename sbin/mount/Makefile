#	@(#)Makefile	8.6 (Berkeley) 5/8/95
# $FreeBSD$

PACKAGE=runtime
PROG=	mount
SRCS=	mount.c mount_fs.c getmntopts.c vfslist.c
MAN=	mntopts.3 mount.8
MLINKS+= mntopts.3 getmntopts.3
MLINKS+= mntopts.3 getmntpoint.3
MLINKS+= mntopts.3 chkdoreload.3
MLINKS+= mntopts.3 build_iovec.3
MLINKS+= mntopts.3 build_iovec_argf.3
MLINKS+= mntopts.3 free_iovec.3
MLINKS+= mntopts.3 checkpath.3
MLINKS+= mntopts.3 rmslashes.3

LIBADD=	util xo

.include <bsd.prog.mk>
