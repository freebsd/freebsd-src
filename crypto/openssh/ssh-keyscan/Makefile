#	$OpenBSD: Makefile,v 1.4 2001/08/05 23:18:20 markus Exp $

.PATH:		${.CURDIR}/..

PROG=	ssh-keyscan
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/bin
MAN=	ssh-keyscan.1

SRCS=	ssh-keyscan.c

.include <bsd.prog.mk>

LDADD+= -lcrypto -lz
DPADD+= ${LIBCRYPTO} ${LIBZ}
