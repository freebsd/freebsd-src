# $FreeBSD$

MAINTAINER=brian@FreeBSD.org

PROG=	ppp
SRCS=	acf.c arp.c async.c auth.c bundle.c cbcp.c ccp.c chap.c chat.c \
	command.c datalink.c deflate.c defs.c exec.c filter.c fsm.c hdlc.c \
	id.c iface.c ip.c ipcp.c iplist.c lcp.c link.c log.c lqr.c main.c \
	mbuf.c mp.c pap.c physical.c pred.c probe.c prompt.c proto.c route.c \
	server.c sig.c slcompress.c sync.c systems.c tcp.c throughput.c \
	timer.c tty.c tun.c udp.c vjcomp.c
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

.if defined(NOALIAS) || defined(NONAT)
CFLAGS+=-DNONAT
.else
.if !defined(RELEASE_CRUNCH)
SRCS+=	nat_cmd.c
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

.if defined(NOI4B) || ${MACHINE_ARCH} == "alpha"
CFLAGS+=-DNOI4B
.else
SRCS+=	i4b.c
.endif

.if defined(RELEASE_CRUNCH)
# We must create these objects because crunchgen will link them,
# and we don't want any unused symbols to spoil the final link.
CFLAGS+=-DNONAT -DNORADIUS -DNOI4B
OBJS+=	nat_cmd.o chap_ms.o radius.o
chap_ms.o nat_cmd.o radius.o:
	>null_${.PREFIX}.c
	cc -c -o ${.TARGET} null_${.PREFIX}.c
.endif

.include <bsd.prog.mk>
