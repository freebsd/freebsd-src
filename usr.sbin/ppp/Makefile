#	$Id: Makefile,v 1.52 1999/01/28 15:16:38 brian Exp $

MAINTAINER=brian@FreeBSD.org

PROG=	ppp
SRCS=	arp.c async.c auth.c bundle.c cbcp.c ccp.c chap.c chat.c command.c \
	datalink.c deflate.c defs.c filter.c fsm.c hdlc.c id.c iface.c ip.c \
	ipcp.c iplist.c lcp.c link.c log.c lqr.c main.c mbuf.c modem.c \
	mp.c pap.c physical.c pred.c probe.c prompt.c route.c server.c \
	sig.c slcompress.c systems.c throughput.c timer.c tun.c vjcomp.c
CFLAGS+=-Wall
LDADD+= -lcrypt -lmd -lutil -lz
DPADD+=	${LIBCRYPT} ${LIBMD} ${LIBUTIL} ${LIBZ}
BINMODE=4554
BINOWN=	root
BINGRP=	network
MAN8=	ppp.8

.if defined(RELEASE_CRUNCH)
CFLAGS+=-DRELEASE_CRUNCH
.endif

.if defined(NOALIAS)
CFLAGS+=-DNOALIAS
.else
.if !defined(RELEASE_CRUNCH)
SRCS+=	alias_cmd.c
LDADD+=	-lalias
DPADD+= ${LIBALIAS}
.endif
.endif

.if exists(${.CURDIR}/../../secure) && !defined(NOCRYPT) && !defined(NOSECURE) && !defined(RELEASE_CRUNCH)
DISTRIBUTION=des
CFLAGS+=-DHAVE_DES
SRCS+=	chap_ms.c
LDADD+= -ldes
DPADD+= ${LIBDES}
.endif

.if defined(NORADIUS)
CFLAGS+=-DNORADIUS
.else
.if !defined(RELEASE_CRUNCH)
SRCS+=	radius.c
LDADD+=	-lradius
DPADD+= ${LIBRADIUS}
.endif
.endif

.if defined(RELEASE_CRUNCH)
# We must create these objects because crunchgen will link them,
# and we don't want any unused symbols to spoil the final link.
CFLAGS+=-DNOALIAS -DNORADIUS
SRCS+=	alias_cmd.c chap_ms.c radius.c
chap_ms.o alias_cmd.o radius.o:
	>null_${.PREFIX}.c
	cc -c -o ${.TARGET} null_${.PREFIX}.c
.endif

.include <bsd.prog.mk>
