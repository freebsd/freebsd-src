# $FreeBSD$

PROG=	ppp
MAN=	ppp.8
SRCS=	acf.c arp.c async.c auth.c bundle.c cbcp.c ccp.c chap.c chat.c \
	command.c datalink.c deflate.c defs.c exec.c filter.c fsm.c hdlc.c \
	iface.c ip.c ipcp.c ipv6cp.c iplist.c lcp.c link.c log.c lqr.c main.c \
	mbuf.c mp.c ncp.c ncpaddr.c pap.c physical.c pred.c probe.c prompt.c \
	proto.c route.c server.c sig.c slcompress.c sync.c systems.c tcp.c \
	tcpmss.c throughput.c timer.c tty.c tun.c udp.c vjcomp.c
.if defined(RELEASE_CRUNCH)
CFLAGS+=-DRELEASE_CRUNCH
NOATM=		true
NODES=		true
NOI4B=		true
NONAT=		true
NOKLDLOAD=	true
NORADIUS=	true
NOSUID=		true
.endif

.if defined(NOSUID) || defined(PPP_NOSUID)
BINMODE=550
.else
BINMODE=4550
BINOWN=	root
.endif
BINGRP=	network
M4FLAGS=

LDADD= -lcrypt -lmd -lutil -lz
DPADD=	${LIBCRYPT} ${LIBMD} ${LIBUTIL} ${LIBZ}

.SUFFIXES: .8 .8.m4

.8.m4.8:
	m4 ${M4FLAGS} ${.IMPSRC} >${.TARGET}

CLEANFILES= ppp.8

.if defined(PPP_CONFDIR) && !empty(PPP_CONFDIR)
CFLAGS+=-DPPP_CONFDIR=\"${PPP_CONFDIR}\"
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
SRCS+=	nat_cmd.c
LDADD+=	-lalias
DPADD+= ${LIBALIAS}
.endif

.if defined(NOATM)
CFLAGS+=-DNOATM
.else
SRCS+=	atm.c
.endif

.if defined(NOSUID) || defined(PPP_NOSUID)
CFLAGS+=-DNOSUID
.else
SRCS+=	id.c
.endif

.if !exists(${.CURDIR}/../../crypto) || defined(NOCRYPT) || defined(NO_OPENSSL) || defined(NODES)
CFLAGS+=-DNODES
.else
DISTRIBUTION=crypto
SRCS+=	chap_ms.c mppe.c
LDADD+= -lcrypto
DPADD+= ${LIBCRYPTO}
.endif

.if defined(NORADIUS)
CFLAGS+=-DNORADIUS
.else
SRCS+=	radius.c
LDADD+=	-lradius
DPADD+= ${LIBRADIUS}
.endif

.if defined(NOI4B) || ${MACHINE_ARCH} != "i386"
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
.if defined(EXPERIMENTAL_NETGRAPH)
CFLAGS+=-DEXPERIMENTAL_NETGRAPH
SRCS+=	netgraph.c
.endif
.endif

.include <bsd.prog.mk>
