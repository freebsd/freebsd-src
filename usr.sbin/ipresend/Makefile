# $FreeBSD: src/usr.sbin/ipresend/Makefile,v 1.4.2.3 2001/11/14 14:15:04 guido Exp $

.PATH:	${.CURDIR}/../../contrib/ipfilter/ipsend \
	${.CURDIR}/../../contrib/ipfilter

PROG=	ipresend
SRCS=	ipresend.c ip.c resend.c opt.c ipft_ef.c ipft_hx.c ipft_sn.c ipft_td.c ipft_tx.c sbpf.c 44arp.c ipft_pc.c

CFLAGS+=-DDOSOCKET -DIPL_NAME=\"/dev/ipl\" -DUSE_INET6 \
	-I- -I${.CURDIR}/../../sys/contrib/ipfilter/netinet \
	-I${.CURDIR}/../../contrib/ipfilter/ipsend \
	-I${.CURDIR}/../../contrib/ipfilter

MAINTAINER=	guido@freebsd.org

.include <bsd.prog.mk>
