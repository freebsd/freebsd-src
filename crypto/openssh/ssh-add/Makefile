#	$OpenBSD: Makefile,v 1.20 2001/03/04 00:51:25 markus Exp $

.PATH:		${.CURDIR}/..

PROG=	ssh-add
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/bin
MAN=	ssh-add.1

SRCS=	ssh-add.c

.include <bsd.prog.mk>

LDADD+=	-lcrypto
DPADD+= ${LIBCRYPTO}
