#	$Id: Makefile,v 1.9.2.14 1998/01/29 00:44:14 brian Exp $

PROG=	ppp
SRCS=	arp.c async.c auth.c ccp.c chap.c chat.c command.c deflate.c \
	defs.c filter.c fsm.c hdlc.c id.c ip.c ipcp.c iplist.c lcp.c \
	log.c lqr.c main.c mbuf.c modem.c os.c pap.c phase.c \
	pred.c route.c server.c sig.c slcompress.c systems.c throughput.c \
	timer.c tun.c vars.c vjcomp.c
CFLAGS+=-Wall
LDADD+= -lmd -lcrypt -lutil -lz
DPADD+= ${LIBMD} ${LIBCRYPT} ${LIBUTIL} ${LIBZ}
MAN8=	ppp.8
BINMODE=4551
BINOWN=	root
BINGRP=	network

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
	>null_$*.c
	cc -c -o $@ null_$*.c
.endif

.include <bsd.prog.mk>
