# $FreeBSD$

.PATH: ${SRCTOP}/contrib/ntp/sntp/libopts

LIB=	opts
INTERNALLIB=

# XXX libopts uses generated format string tables containing embedded NULs.
NO_WFORMAT=

SRCS=	libopts.c

CFLAGS+= -I${SRCTOP}/contrib/ntp/include \
	-I${SRCTOP}/contrib/ntp/sntp/libopts \
	-I${.CURDIR:H}

.include <bsd.lib.mk>
