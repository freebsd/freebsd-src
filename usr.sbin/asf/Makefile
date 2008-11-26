# $FreeBSD: src/usr.sbin/asf/Makefile,v 1.2.10.1.6.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	asf
SRCS=	asf.c asf_kld.c asf_kvm.c asf_prog.c
MAN=	asf.8

DPADD=	${LIBKVM}
LDADD=	-lkvm

WARNS?=	4

.include <bsd.prog.mk>
