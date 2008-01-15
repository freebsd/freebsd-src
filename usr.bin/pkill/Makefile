#	$NetBSD: Makefile,v 1.1 2002/03/01 11:21:58 ad Exp $
# $FreeBSD: src/usr.bin/pkill/Makefile,v 1.5.2.1 2006/09/01 14:32:35 yar Exp $

BINDIR=	/bin

PROG=	pkill
WARNS?=	5

DPADD=	${LIBKVM}
LDADD=	-lkvm

LINKS=	${BINDIR}/pkill ${BINDIR}/pgrep
MLINKS=	pkill.1 pgrep.1

#
# If considering retirement of these compatibility symlinks,
# keep in mind that pkill is installed to /usr/bin in other
# OS types, e.g., NetBSD, OpenBSD, Solaris, and Linux.
#
SYMLINKS=	${BINDIR}/pkill /usr/bin/pkill
SYMLINKS+=	${BINDIR}/pgrep /usr/bin/pgrep

.include <bsd.prog.mk>
