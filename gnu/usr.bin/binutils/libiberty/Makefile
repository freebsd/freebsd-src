# $FreeBSD$

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/libiberty

LIB=	iberty
SRCS=	argv.c choose-temp.c concat.c cp-demangle.c cplus-dem.c \
	dyn-string.c getopt.c getopt1.c getpwd.c getruntime.c \
	hex.c floatformat.c lbasename.c objalloc.c obstack.c safe-ctype.c \
	xatexit.c xexit.c xmalloc.c \
	xstrdup.c xstrerror.c
.if defined(BOOTSTRAPPING)
SRCS+=	basename.c
.endif
CFLAGS+=	-DHAVE_CONFIG_H
NOPROFILE=	true
NOPIC=		true
INTERNALLIB=	true
INTERNALSTATICLIB=true

.include <bsd.lib.mk>
