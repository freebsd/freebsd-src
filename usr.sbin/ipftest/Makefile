# $FreeBSD: src/usr.sbin/ipftest/Makefile,v 1.4.2.5 2002/04/27 18:19:04 darrenr Exp $

.PATH:	${.CURDIR}/../../sys/netinet \
	${.CURDIR}/../../contrib/ipfilter ${.CURDIR}/../../contrib/ipfilter/man

PROG=	ipftest
SRCS=	opt_ipfilter.h \
	ipt.c parse.c fil.c ipft_sn.c ipft_ef.c ipft_td.c ipft_pc.c opt.c \
	ipft_tx.c misc.c ip_frag.c ip_state.c ip_nat.c ip_proxy.c ip_log.c \
	ip_auth.c ipft_hx.c ip_fil.c natparse.c facpri.c common.c \
	printstate.c printnat.c

CFLAGS+=-DUSE_INET6 -DIPL_NAME=\"/dev/ipl\" -DIPFILTER_LOG
CFLAGS+=-I- -I${.OBJDIR}
CFLAGS+=-I${.CURDIR}/../../sys/contrib/ipfilter
CFLAGS+=-I${.CURDIR}/../../sys
CFLAGS+=-I${.CURDIR}/../../contrib/ipfilter

CLEANFILES+=	opt_ipfilter.h

opt_ipfilter.h:	Makefile
	echo "#define IPFILTER 1" > opt_ipfilter.h

MAINTAINER=	guido@freebsd.org

.include <bsd.prog.mk>
