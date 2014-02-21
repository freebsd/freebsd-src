# $FreeBSD$

.PATH:	${.CURDIR}/../../contrib/dma

CFLAGS=	-I${.CURDIR}/../../contrib/dma \
	-DHAVE_REALLOCF -DHAVE_STRLCPY -DHAVE_GETPROGNAME \
	-DCONF_PATH='"/etc/dma"' \
	-DLIBEXEC_PATH='"/usr/libexec"' -DDMA_VERSION='"v0.9+"'
DPADD=	${LIBSSL} ${LIBCRYPTO}
LDADD=	-lssl -lcrypto

PROG=	dma
SRCS=	aliases_parse.y \
	aliases_scan.l \
	base64.c \
	conf.c \
	crypto.c \
	dma.c \
	dns.c \
	local.c \
	mail.c \
	net.c \
	spool.c \
	util.c
MAN8=	dma.8
YFLAGS+=	-i
CLEANFILES=	aliases_parse.i

BINGRP=	mail
BINMODE=	2555

.include <bsd.prog.mk>
