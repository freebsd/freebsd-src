# $FreeBSD$

.PATH:	${.CURDIR}/../../contrib/ipfilter/ipsend \
	${.CURDIR}/../../contrib/ipfilter/iplang \
	${.CURDIR}/../../contrib/ipfilter

PROG=	ipsend
MAN1=	ipsend.1
MAN5=	ipsend.5
SRCS=	ipsend.c ip.c ipsopt.c sbpf.c sock.c 44arp.c iplang_y.y iplang_l.l

CFLAGS+=-DDOSOCKET -DIPL_NAME=\"/dev/ipl\" \
	-I- -I. -I${.CURDIR}/../../sys/netinet \
	-I${.CURDIR}/../../contrib/ipfilter/ipsend \
	-I${.CURDIR}/../../contrib/ipfilter/iplang \
	-I${.CURDIR}/../../contrib/ipfilter

DPADD=	${LIBL}
LDADD=	-ll

.include <bsd.prog.mk>
