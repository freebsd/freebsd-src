# $FreeBSD: src/usr.sbin/asf/Makefile,v 1.3.10.1.2.1 2009/10/25 01:10:29 kensmith Exp $

PROG=	asf
SRCS=	asf.c asf_kld.c asf_kvm.c asf_prog.c
MAN=	asf.8

DPADD=	${LIBKVM}
LDADD=	-lkvm

WARNS?=	4

.include <bsd.prog.mk>
