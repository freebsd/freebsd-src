# $FreeBSD$

.PATH: ${.CURDIR}/../../sys/contrib/ipfilter/netinet \
       ${.CURDIR}/../../contrib/ipfilter ${.CURDIR}/../../contrib/ipfilter/man

PROG=	ipftest
SRCS=	ipt.c parse.c fil.c ipft_sn.c ipft_ef.c ipft_td.c ipft_pc.c opt.c \
	ipft_tx.c misc.c ip_frag.c ip_state.c ip_nat.c ip_proxy.c \
	ip_log.c ip_auth.c ipft_hx.c ip_fil.c natparse.c facpri.c common.c \
	printstate.c printnat.c

CFLAGS+= -DUSE_INET6 -DIPL_NAME=\"/dev/ipl\" -DIPFILTER_LOG
CFLAGS+= -I${.CURDIR}/../../sys/contrib/ipfilter/netinet
CFLAGS+= -I${.CURDIR}/../../sys/contrib/ipfilter
CFLAGS+= -I${.CURDIR}/../../contrib/ipfilter

.include <bsd.prog.mk>
