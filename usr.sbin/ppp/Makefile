# $FreeBSD$

PROG=	ppp
SRCS=	acf.c arp.c async.c auth.c bundle.c cbcp.c ccp.c chap.c chat.c \
	command.c datalink.c deflate.c defs.c exec.c filter.c fsm.c hdlc.c \
	iface.c ip.c ipcp.c iplist.c lcp.c link.c log.c lqr.c main.c \
	mbuf.c mp.c pap.c physical.c pred.c probe.c prompt.c proto.c route.c \
	server.c sig.c slcompress.c sync.c systems.c tcp.c throughput.c \
	timer.c tty.c tun.c udp.c vjcomp.c
CFLAGS+=-Wall
LDADD+= -lcrypt -lmd -lutil -lz
DPADD+=	${LIBCRYPT} ${LIBMD} ${LIBUTIL} ${LIBZ}
.if defined(NOSUID) || defined(PPP_NOSUID)
BINMODE=554
.else
BINMODE=4554
BINOWN=	root
.endif
BINGRP=	network
MAN8=	ppp.8

.if defined(RELEASE_CRUNCH)
CFLAGS+=-DRELEASE_CRUNCH
.endif

.if defined(NOKLDLOAD)
CFLAGS+=-DNOKLDLOAD
.endif

.if defined(NOINET6)
CFLAGS+=-DNOINET6
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

.if defined(NOATM) || defined(RELEASE_CRUNCH)
CFLAGS+=-DNOATM
.else
SRCS+=	atm.c
.endif

.if defined(NOSUID) || defined(PPP_NOSUID)
CFLAGS+=-DNOSUID
.else
.if !defined(RELEASE_CRUNCH)
SRCS+=	id.c
.endif
.endif

.if exists(${.CURDIR}/../../secure) && !defined(NOCRYPT) && !defined(NOSECURE) && !defined(NO_OPENSSL) && !defined(RELEASE_CRUNCH)
DISTRIBUTION=crypto
CFLAGS+=-DHAVE_DES
SRCS+=	chap_ms.c mppe.c
LDADD+= -lcrypto
DPADD+= ${LIBCRYPTO}
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

.if defined(NONETGRAPH)
CFLAGS+=-DNONETGRAPH
.else
SRCS+=	ether.c
LDADD+=	-lnetgraph
DPADD+= ${LIBNETGRAPH}
.endif

.if defined(RELEASE_CRUNCH)
# We must create these objects because crunchgen will link them,
# and we don't want any unused symbols to spoil the final link.
CFLAGS+=-DNONAT -DNORADIUS -DNOI4B -DNOSUID
OBJS+=	chap_ms.o mppe.o id.o nat_cmd.o radius.o
chap_ms.o mppe.o id.o nat_cmd.o radius.o:
	>null_${.PREFIX}.c
	cc -c -o ${.TARGET} null_${.PREFIX}.c
.endif

.include <bsd.prog.mk>
