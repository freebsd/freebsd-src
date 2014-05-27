# $FreeBSD$

.PATH: ${.CURDIR}/../../contrib/dma

CFLAGS=	-I${.CURDIR}/../../contrib/dma \
	-DHAVE_REALLOCF -DHAVE_STRLCPY -DHAVE_GETPROGNAME \
	-DCONF_PATH='"/etc/dma"' \
	-DLIBEXEC_PATH='"/usr/libexec"' -DDMA_VERSION='"v0.9+"'

MAN=

WARNS=	2

PROG=	dma-mbox-create
BINGRP=	mail
BINMODE=	4554

.include <bsd.prog.mk>
