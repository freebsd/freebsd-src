# $FreeBSD$

# as per handbook policies section
MAINTAINER=	peter@freebsd.org

PROG=	pppd
MAN=	pppd.8
SRCS=	main.c magic.c fsm.c lcp.c ipcp.c ipxcp.c upap.c chap.c ccp.c \
	demand.c auth.c options.c sys-bsd.c
BINMODE=4550
BINOWN=	root
BINGRP=	dialer

CFLAGS+= -DHAVE_PATHS_H

DPADD=	${LIBCRYPT} ${LIBUTIL} ${LIBMD}
LDADD=	-lcrypt -lutil -lmd

# Support SPX/IPX - not quite ready
#CFLAGS+= -DIPX_CHANGE
#SRCS+=	ipxcp.c

# Callback Control Protocol
CFLAGS+= -DCBCP_SUPPORT
SRCS+=	cbcp.c

# Filter support
CFLAGS+= -DPPP_FILTER
DPADD+=	${LIBPCAP}
LDADD+=	-lpcap

# MS-CHAP support.  Requires the DES library.
.if exists(${.CURDIR}/../../crypto) && !defined(NOCRYPT) && !defined(NO_OPENSSL) && !defined(RELEASE_CRUNCH)
DISTRIBUTION=crypto
CFLAGS+= -DCHAPMS
SRCS+=	chap_ms.c
LDADD+=	-lcrypto
DPADD+=	${LIBCRYPTO}
.endif

.if defined(RELEASE_CRUNCH)
# We must create these objects because crunchgen will link them,
# and we don't want any unused symbols to spoil the final link.
SRCS+= chap_ms.c
chap_ms.o:
	>null_${.PREFIX}.c
	cc -c -o ${.TARGET} null_${.PREFIX}.c
.endif

.include <bsd.prog.mk>
