#	$Id$
#	$NetBSD: Makefile,v 1.11 1997/03/24 21:59:36 christos Exp $
#	from: @(#)Makefile	8.2 (Berkeley) 4/3/94

PROG=	ftp
SRCS=	cmds.c cmdtab.c complete.c domacro.c fetch.c ftp.c main.c ruserpass.c \
	util.c

LDADD+=	-ledit -ltermcap
DPADD+=	${LIBEDIT} ${LIBTERMCAP}

LINKS=	${BINDIR}/ftp ${BINDIR}/pftp
MLINKS=	ftp.1 pftp.1

.include <bsd.prog.mk>
