#	$OpenBSD: Makefile,v 1.21 2001/06/27 19:29:16 markus Exp $

.PATH:		${.CURDIR}/..

PROG=	ssh-agent
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/bin
MAN=	ssh-agent.1

SRCS=	ssh-agent.c

.include <bsd.prog.mk>

LDADD+=	-lcrypto
DPADD+=	${LIBCRYPTO}
