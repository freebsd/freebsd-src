# $FreeBSD: src/usr.sbin/iptest/Makefile,v 1.4.2.3 2001/11/14 14:15:05 guido Exp $

.PATH:	${.CURDIR}/../../contrib/ipfilter/ipsend \
	${.CURDIR}/../../contrib/ipfilter

PROG=	iptest
SRCS=	iptest.c iptests.c ip.c sbpf.c 44arp.c sock.c

CFLAGS+=-DDOSOCKET -DIPL_NAME=\"/dev/ipl\" -DUSE_INET6 \
	-I- -I${.CURDIR}/../../sys/contrib/ipfilter/netinet \
	-I${.CURDIR}/../../contrib/ipfilter/ipsend \
	-I${.CURDIR}/../../contrib/ipfilter

MAINTAINER=	guido@freebsd.org

.include <bsd.prog.mk>
