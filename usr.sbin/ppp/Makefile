#	$Id: Makefile,v 1.41 1998/06/09 05:36:37 imp Exp $

PROG=	ppp
SRCS=	arp.c async.c auth.c bundle.c ccp.c chap.c chat.c command.c \
	datalink.c deflate.c defs.c filter.c fsm.c hdlc.c id.c ip.c \
	ipcp.c iplist.c lcp.c link.c log.c lqr.c main.c mbuf.c modem.c \
	mp.c pap.c physical.c pred.c probe.c prompt.c route.c server.c \
	sig.c slcompress.c systems.c throughput.c timer.c tun.c vjcomp.c
CFLAGS+=-Wall
LDADD+= -lutil -lz
DPADD+=	${LIBUTIL} ${LIBZ}
BINMODE=4554
BINOWN=	root
BINGRP=	network

OPSYS!=	uname -s
.if (${OPSYS} == "OpenBSD")
MAN=	ppp.8
.else
MAN8=	ppp.8
LDADD+=	-lmd -lcrypt
DPADD+= ${LIBMD} ${LIBCRYPT}
.endif

.if defined(RELEASE_CRUNCH)
CFLAGS+=-DRELEASE_CRUNCH
.endif

.if defined(NOALIAS) || defined(RELEASE_CRUNCH)
CFLAGS+=-DNOALIAS
.else
SRCS+=	alias_cmd.c loadalias.c
.endif

.if exists(${.CURDIR}/../../secure) && !defined(NOCRYPT) && !defined(NOSECURE) && !defined(RELEASE_CRUNCH)
CFLAGS+=-DHAVE_DES
SRCS+=	chap_ms.c
LDADD+= -ldes
DPADD+= ${LIBDES}
.endif

.if defined(RELEASE_CRUNCH)
# We must create these objects because the crunchgen will link them,
# and we don't want any unused symbols to spoil the final link.
SRCS+=	alias_cmd.c loadalias.c chap_ms.c
chap_ms.o alias_cmd.o loadalias.o:
	>null_${.PREFIX}.c
	cc -c -o ${.TARGET} null_${.PREFIX}.c
.endif

.include <bsd.prog.mk>
