#	$OpenBSD: Makefile,v 1.13 2008/02/14 01:49:17 mcbride Exp $

PROG=	authpf
MAN=	authpf.8

LINKS=  ${BINDIR}/authpf ${BINDIR}/authpf-noip
MLINKS+=authpf.8 authpf-noip.8

BINOWN= root
BINGRP= authpf
BINMODE= 6555
SRCS=	authpf.c
CFLAGS+= -Wall

.include <bsd.prog.mk>
