# $FreeBSD$

MAINTAINER=	darrenr@freebsd.org

.PATH: ${.CURDIR}/../../contrib/ipfilter/ipsend \
       ${.CURDIR}/../../contrib/ipfilter/iplang \
       ${.CURDIR}/../../contrib/ipfilter

PROG=	ipsend
MAN=	ipsend.1 ipsend.5
SRCS=	ipsend.c ip.c ipsopt.c sbpf.c sock.c 44arp.c iplang_y.y iplang_l.l \
	y.tab.h

CFLAGS+= -DDOSOCKET -DIPL_NAME=\"/dev/ipl\" -DUSE_INET6
CFLAGS+= -I${.CURDIR}/../../sys/contrib/ipfilter/netinet
CFLAGS+= -I. -I${.CURDIR}/../../contrib/ipfilter/ipsend \
	 -I${.CURDIR}/../../contrib/ipfilter/iplang \
	 -I${.CURDIR}/../../contrib/ipfilter

DPADD=	${LIBL}
LDADD=	-ll

.include <bsd.prog.mk>
