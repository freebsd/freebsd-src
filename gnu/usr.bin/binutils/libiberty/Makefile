# $FreeBSD$

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/libiberty

LIB=	iberty
SRCS=	argv.c choose-temp.c concat.c cp-demangle.c cplus-dem.c \
	dyn-string.c getopt.c getopt1.c getruntime.c \
	hex.c floatformat.c objalloc.c obstack.c \
	xatexit.c xexit.c xmalloc.c \
	xstrdup.c xstrerror.c
CFLAGS+=	-DHAVE_CONFIG_H
NOPROFILE=	true
NOPIC=		true
INTERNALLIB=	true
INTERNALSTATICLIB=true

.include <bsd.lib.mk>
