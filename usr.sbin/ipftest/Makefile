# $FreeBSD$

.PATH:	${.CURDIR}/../../sys/netinet \
	${.CURDIR}/../../contrib/ipfilter ${.CURDIR}/../../contrib/ipfilter/man

PROG=	ipftest
MAN1=	ipftest.1
SRCS=	opt_ipfilter.h \
	ipt.c fil.c ipft_hx.c ipft_sn.c ipft_ef.c ipft_td.c ipft_pc.c \
	ipft_tx.c misc.c parse.c opt.c ip_frag.c ip_nat.c ip_state.c \
	ip_auth.c ip_fil.c ip_proxy.c

CFLAGS+=-DIPL_NAME=\"/dev/ipl\" -I- -I${.OBJDIR} -I${.CURDIR}/../../sys/netinet -I${.CURDIR}/../../sys -I${.CURDIR}/../../contrib/ipfilter

CLEANFILES+=	opt_ipfilter.h

opt_ipfilter.h:	Makefile
	echo "#define IPFILTER 1" > opt_ipfilter.h

.include <bsd.prog.mk>
