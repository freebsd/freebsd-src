# $FreeBSD: src/usr.sbin/iptest/Makefile,v 1.11 2003/06/06 11:30:50 guido Exp $

MAINTAINER=	darrenr@freebsd.org

.PATH: ${.CURDIR}/../../contrib/ipfilter/ipsend \
       ${.CURDIR}/../../contrib/ipfilter

PROG=	iptest
SRCS=	iptest.c iptests.c ip.c sbpf.c 44arp.c sock.c

CFLAGS+= -DDOSOCKET -DIPL_NAME=\"/dev/ipl\" -DUSE_INET6
CFLAGS+= -I${.CURDIR}/../../sys/contrib/ipfilter/netinet
CFLAGS+= -I${.CURDIR}/../../contrib/ipfilter/ipsend \
	 -I${.CURDIR}/../../contrib/ipfilter

.include <bsd.prog.mk>
