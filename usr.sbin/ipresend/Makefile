# $FreeBSD: src/usr.sbin/ipresend/Makefile,v 1.4 1999/12/06 20:50:01 guido Exp $

.PATH:	${.CURDIR}/../../contrib/ipfilter/ipsend \
	${.CURDIR}/../../contrib/ipfilter

PROG=	ipresend
MAN1=	ipresend.1
SRCS=	ipresend.c ip.c resend.c opt.c ipft_ef.c ipft_hx.c ipft_sn.c ipft_td.c ipft_tx.c sbpf.c 44arp.c ipft_pc.c

CFLAGS+=-DDOSOCKET -DIPL_NAME=\"/dev/ipl\" \
	-I- -I${.CURDIR}/../../sys/netinet \
	-I${.CURDIR}/../../contrib/ipfilter/ipsend \
	-I${.CURDIR}/../../contrib/ipfilter

MAINTAINER=	guido@freebsd.org

.include <bsd.prog.mk>
