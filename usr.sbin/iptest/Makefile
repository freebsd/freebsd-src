# $FreeBSD: src/usr.sbin/iptest/Makefile,v 1.4 1999/12/06 20:50:03 guido Exp $

.PATH:	${.CURDIR}/../../contrib/ipfilter/ipsend \
	${.CURDIR}/../../contrib/ipfilter

PROG=	iptest
MAN1=	iptest.1
SRCS=	iptest.c iptests.c ip.c sbpf.c 44arp.c sock.c

CFLAGS+=-DDOSOCKET -DIPL_NAME=\"/dev/ipl\" \
	-I- -I${.CURDIR}/../../sys/netinet \
	-I${.CURDIR}/../../contrib/ipfilter/ipsend \
	-I${.CURDIR}/../../contrib/ipfilter

MAINTAINER=	guido@freebsd.org

.include <bsd.prog.mk>
