# $FreeBSD$

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/libiberty

LIB=	iberty
SRCS=	argv.c choose-temp.c concat.c cp-demangle.c cplus-dem.c \
	dyn-string.c getopt.c getopt1.c getpwd.c getruntime.c \
	hex.c floatformat.c hashtab.c lbasename.c make-temp-file.c \
	objalloc.c obstack.c safe-ctype.c xatexit.c xexit.c xmalloc.c \
	xstrdup.c xstrerror.c
.if defined(BOOTSTRAPPING) && \
    ( ${BOOTSTRAPPING} < 440000 || \
    ( ${BOOTSTRAPPING} >= 500000 && ${BOOTSTRAPPING} < 500022 ))
SRCS+=	basename.c
.endif
WARNS=	1
CFLAGS+=	-DHAVE_CONFIG_H
INTERNALLIB=	true

.include <bsd.lib.mk>
