# $FreeBSD$

.PATH: ${.CURDIR}/../../../contrib/ntp/arlib

LIB= ares

SRCS=	arlib.c 

CFLAGS+= -DPACKAGE=\"arlib\" -I${.CURDIR}/../../../contrib/ntp/arlib \
		-DNS_ADDR_LIST=nsaddr_list -DVERSION=\"1.0\"

.include <bsd.lib.mk>
