#
# $FreeBSD: src/gnu/usr.bin/binutils/libiberty/Makefile,v 1.3.2.1 2000/07/06 22:16:08 obrien Exp $
#

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/libiberty

LIB=		iberty
SRCS=		argv.c basename.c choose-temp.c concat.c cplus-dem.c \
		fdmatch.c fnmatch.c getopt.c getopt1.c getruntime.c \
		hex.c floatformat.c objalloc.c obstack.c spaces.c \
		strerror.c strsignal.c xatexit.c xexit.c xmalloc.c \
		xstrdup.c xstrerror.c insque.c
CFLAGS+=	-DHAVE_CONFIG_H
NOPROFILE=	true
NOPIC=		true
INTERNALLIB=	true
INTERNALSTATICLIB=true

.include <bsd.lib.mk>
