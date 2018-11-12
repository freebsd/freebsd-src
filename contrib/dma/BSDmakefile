# $DragonFly: src/libexec/dma/Makefile,v 1.5 2008/09/19 00:36:57 corecode Exp $
#

version!=	sh get-version.sh

CFLAGS+= -I${.CURDIR}
CFLAGS+= -DHAVE_REALLOCF -DHAVE_STRLCPY -DHAVE_GETPROGNAME
CFLAGS+= -DLIBEXEC_PATH='"${LIBEXEC}"' -DDMA_VERSION='"${version}"'
CFLAGS+= -DCONF_PATH='"${CONFDIR}"'

DPADD=  ${LIBSSL} ${LIBCRYPTO}
LDADD=  -lssl -lcrypto

PROG=	dma
SRCS=	aliases_parse.y aliases_scan.l base64.c conf.c crypto.c
SRCS+=	dma.c dns.c local.c mail.c net.c spool.c util.c
MAN=	dma.8

PREFIX?=	/usr/local
LIBEXEC?=	${PREFIX}/libexec
CONFDIR?=	${PREFIX}/etc/dma

BINOWN= root
BINGRP= mail
BINMODE=2555
WARNS?=	6

.include <bsd.prog.mk>
