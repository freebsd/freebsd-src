#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/kgmon/Makefile,v 1.6.24.1 2010/02/10 00:26:20 kensmith Exp $

PROG=	kgmon
MAN=	kgmon.8
WARNS?=	2

# This program may safely be run setuid-root to allow non-root
# users to start, stop, and reset profiling buffers.
#
#BINOWN=root
#BINMODE=4555

DPADD=	${LIBKVM}
LDADD=	-lkvm

.include <bsd.prog.mk>
