# $FreeBSD$

.PATH: ${.CURDIR}/../../contrib/ipfilter/ipsend \
       ${.CURDIR}/../../contrib/ipfilter

PROG=	ipresend
SRCS=	ipresend.c ip.c resend.c opt.c ipft_ef.c ipft_hx.c ipft_sn.c ipft_td.c \
	ipft_tx.c sbpf.c 44arp.c ipft_pc.c

CFLAGS+= -DDOSOCKET -DIPL_NAME=\"/dev/ipl\" -DUSE_INET6
CFLAGS+= -I${.CURDIR}/../../sys/contrib/ipfilter/netinet
CFLAGS+= -I${.CURDIR}/../../contrib/ipfilter/ipsend \
	 -I${.CURDIR}/../../contrib/ipfilter

.include <bsd.prog.mk>
