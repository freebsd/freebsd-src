# $FreeBSD: src/usr.sbin/asf/Makefile,v 1.3.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	asf
SRCS=	asf.c asf_kld.c asf_kvm.c asf_prog.c
MAN=	asf.8

DPADD=	${LIBKVM}
LDADD=	-lkvm

WARNS?=	4

.include <bsd.prog.mk>
