# $Id: Makefile-bison,v 1.3 1997/02/22 15:44:42 peter Exp $

.PATH:	${.CURDIR}/../../../contrib/cpio

PROG=	cpio
MAN1=	cpio.1

CFLAGS+= -I${.CURDIR} -DRETSIGTYPE=void -DHAVE_SYS_MTIO_H=1 -DSTDC_HEADERS=1 \
	 -DHAVE_UNISTD_H=1 -DHAVE_STRING_H=1 -DHAVE_FCNTL_H=1 -DHAVE_UTIME_H=1 \
	 -DHAVE_STRERROR=1 -DHAVE_VPRINTF=1 -DDIRENT=1 -DHAVE_LCHOWN

SRCS =  copyin.c copyout.c copypass.c defer.c dirname.c dstring.c error.c \
	filemode.c getopt.c getopt1.c global.c idcache.c main.c makepath.c \
	rtapelib.c stripslash.c tar.c userspec.c util.c version.c \
	xmalloc.c xstrdup.c

SRCS+=	lchown.c

.include <bsd.prog.mk>
