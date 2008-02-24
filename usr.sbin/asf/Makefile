# $FreeBSD: src/usr.sbin/asf/Makefile,v 1.3 2006/06/18 11:14:40 yar Exp $

PROG=	asf
SRCS=	asf.c asf_kld.c asf_kvm.c asf_prog.c
MAN=	asf.8

DPADD=	${LIBKVM}
LDADD=	-lkvm

WARNS?=	4

.include <bsd.prog.mk>
