# $FreeBSD: src/usr.sbin/iptest/Makefile,v 1.1 1998/03/21 13:53:18 peter Exp $

.PATH:	${.CURDIR}/../../contrib/ipfilter/ipsend \
	${.CURDIR}/../../contrib/ipfilter

PROG=	iptest
MAN1=	iptest.1
SRCS=	iptest.c iptests.c ip.c sbpf.c 44arp.c sock.c

CFLAGS+=-DDOSOCKET -DIPL_NAME=\"/dev/ipl\" \
	-I- -I${.CURDIR}/../../sys/netinet \
	-I${.CURDIR}/../../contrib/ipfilter/ipsend \
	-I${.CURDIR}/../../contrib/ipfilter

.include <bsd.prog.mk>
