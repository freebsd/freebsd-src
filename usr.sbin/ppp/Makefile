# $FreeBSD$

.include <bsd.own.mk>

PROG=	ppp
MAN=	ppp.8
SRCS=	acf.c arp.c async.c auth.c bundle.c cbcp.c ccp.c chap.c chat.c \
	command.c datalink.c deflate.c defs.c exec.c filter.c fsm.c hdlc.c \
	iface.c ip.c ipcp.c ipv6cp.c iplist.c lcp.c link.c log.c lqr.c main.c \
	mbuf.c mp.c ncp.c ncpaddr.c pap.c physical.c pred.c probe.c prompt.c \
	proto.c route.c server.c sig.c slcompress.c sync.c systems.c tcp.c \
	tcpmss.c throughput.c timer.c tty.c tun.c udp.c vjcomp.c
WARNS?=	3
.if defined(RELEASE_CRUNCH)
CFLAGS+=-DRELEASE_CRUNCH
PPP_NO_ATM=
PPP_NO_DES=
PPP_NO_KLDLOAD=
PPP_NO_NAT=
PPP_NO_PAM=
PPP_NO_RADIUS=
PPP_NO_SUID=
.endif

.if ${MK_ATM} == "no"
PPP_NO_ATM=
.endif
.if ${MK_NETGRAPH} == "no"
PPP_NO_NETGRAPH=
.endif
.if ${MK_PAM_SUPPORT} == "no"
PPP_NO_PAM=
.endif

.if defined(PPP_NO_SUID)
BINMODE=550
.else
BINMODE=4550
BINOWN=	root
.endif
BINGRP=	network
M4FLAGS=

LDADD= -lcrypt -lmd -lutil -lz
DPADD=	${LIBCRYPT} ${LIBMD} ${LIBUTIL} ${LIBZ}

.if defined(PPP_CONFDIR) && !empty(PPP_CONFDIR)
CFLAGS+=-DPPP_CONFDIR=\"${PPP_CONFDIR}\"
.endif

.if defined(PPP_NO_KLDLOAD)
CFLAGS+=-DNOKLDLOAD
.endif

.if ${MK_INET6_SUPPORT} == "no"
CFLAGS+=-DNOINET6
.endif

.if defined(PPP_NO_NAT)
CFLAGS+=-DNONAT
.else
SRCS+=	nat_cmd.c
LDADD+=	-lalias
DPADD+= ${LIBALIAS}
.endif

.if defined(PPP_NO_ATM)
CFLAGS+=-DNOATM
.else
SRCS+=	atm.c
.endif

.if defined(PPP_NO_SUID)
CFLAGS+=-DNOSUID
.else
SRCS+=	id.c
.endif

.if defined(RELEASE_CRUNCH) || ${MK_OPENSSL} == "no" || \
    defined(PPP_NO_DES)
CFLAGS+=-DNODES
.else
SRCS+=	chap_ms.c mppe.c
LDADD+= -lcrypto
DPADD+= ${LIBCRYPTO}
.endif

.if defined(PPP_NO_RADIUS)
CFLAGS+=-DNORADIUS
.else
SRCS+=	radius.c
LDADD+=	-lradius
DPADD+= ${LIBRADIUS}
.endif

.if defined(PPP_NO_NETGRAPH)
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

.if defined(PPP_NO_PAM)
CFLAGS+=-DNOPAM
.else
LDADD+=	${MINUSLPAM}
DPADD+=	${LIBPAM}
.endif

.include <bsd.prog.mk>
