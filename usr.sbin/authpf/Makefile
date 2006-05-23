# $FreeBSD: src/usr.sbin/authpf/Makefile,v 1.1 2004/02/28 21:50:50 mlaier Exp $

.PATH:	${.CURDIR}/../../contrib/pf/authpf
.PATH:	${.CURDIR}/../../contrib/pf/pfctl

PROG=	authpf
MAN=	authpf.8
BINOWN=	root
BINGRP= authpf
BINMODE= 6555

SRCS=   authpf.c parse.y pfctl_parser.c pf_print_state.c pfctl_altq.c
SRCS+=  pfctl_radix.c pfctl_osfp.c

CFLAGS+= -I${.CURDIR}/../../contrib/pf/pfctl -Wall -Werror

# XXX ALTQ:
#CFLAGS+= -DENABLE_ALTQ
CFLAGS+= -I${.CURDIR}/../../sbin/pfctl/missing

LDADD+= -lm -lmd
DPADD+= ${LIBM} ${LIBMD}

CLEANFILES+=y.tab.h

.include <bsd.prog.mk>
