# $FreeBSD: src/usr.sbin/asf/Makefile,v 1.3.14.1 2010/12/21 17:10:29 kensmith Exp $

PROG=	asf
SRCS=	asf.c asf_kld.c asf_kvm.c asf_prog.c
MAN=	asf.8

DPADD=	${LIBKVM}
LDADD=	-lkvm

WARNS?=	4

.include <bsd.prog.mk>
