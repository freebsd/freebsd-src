# Note that this files is under a "BSD" copyright (c) by David O'Brien 1997, 
# even though it may live in src/gnu/...

# $Id: Makefile,v 1.6 1997/04/03 11:20:09 obrien Exp $

.PATH:	${.CURDIR}/../../../contrib/cpio

SUBDIR=	doc

PROG=	cpio
MAN1=	cpio.1

CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../../../contrib/cpio 
CFLAGS+= -DRETSIGTYPE=void -DHAVE_SYS_MTIO_H=1 -DSTDC_HEADERS=1 \
	 -DHAVE_UNISTD_H=1 -DHAVE_STRING_H=1 -DHAVE_FCNTL_H=1 -DHAVE_UTIME_H=1 \
	 -DHAVE_STRERROR=1 -DHAVE_VPRINTF=1 -DDIRENT=1 -DHAVE_LCHOWN \
	 -DHAVE_SYS_PARAM_H=1

SRCS =  copyin.c copyout.c copypass.c defer.c dirname.c dstring.c error.c \
	filemode.c getopt.c getopt1.c global.c idcache.c main.c makepath.c \
	rtapelib.c stripslash.c tar.c userspec.c util.c version.c \
	xmalloc.c xstrdup.c

.include <bsd.prog.mk>
