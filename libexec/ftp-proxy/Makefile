# $FreeBSD: src/libexec/ftp-proxy/Makefile,v 1.1 2004/02/28 21:50:48 mlaier Exp $

.PATH:	${.CURDIR}/../../contrib/pf/ftp-proxy

PROG=	ftp-proxy
MAN=	ftp-proxy.8

SRCS=   ftp-proxy.c getline.c util.c

CFLAGS+= -Wall -Werror

.include <bsd.prog.mk>
