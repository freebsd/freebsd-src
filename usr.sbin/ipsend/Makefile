# $FreeBSD$

.PATH:	${.CURDIR}/../../contrib/ipfilter/ipsend \
	${.CURDIR}/../../contrib/ipfilter/iplang \
	${.CURDIR}/../../contrib/ipfilter

PROG=	ipsend
SRCS=	ipsend.c ip.c ipsopt.c sbpf.c sock.c 44arp.c iplang_y.y iplang_l.l \
	y.tab.h
MAN=	ipsend.1 ipsend.5

CFLAGS+=-DDOSOCKET -DIPL_NAME=\"/dev/ipl\" -DUSE_INET6 \
	-I- -I. -I${.CURDIR}/../../sys/contrib/ipfilter/netinet \
	-I${.CURDIR}/../../contrib/ipfilter/ipsend \
	-I${.CURDIR}/../../contrib/ipfilter/iplang \
	-I${.CURDIR}/../../contrib/ipfilter

DPADD=	${LIBL}
LDADD=	-ll

MAINTAINER=	guido@freebsd.org

.include <bsd.prog.mk>
