# $FreeBSD$
#	$NetBSD: Makefile,v 1.15 1997/10/18 15:31:20 lukem Exp $
#	from: @(#)Makefile	8.2 (Berkeley) 4/3/94

# Uncomment the following to provide defaults for gate-ftp operation
#
#CFLAGS+=-DGATE_SERVER=\"ftp-gw.host\" # -DGATE_PORT=21

LUKEMFTP=	${.CURDIR}/../../contrib/lukemftp
.PATH: ${LUKEMFTP}/src

PROG=	ftp
SRCS=	cmds.c cmdtab.c complete.c domacro.c fetch.c ftp.c main.c ruserpass.c \
	util.c

CFLAGS+=-I${.CURDIR} -I${LUKEMFTP}
LDADD+=	-ledit -ltermcap -lutil
DPADD+=	${LIBEDIT} ${LIBTERMCAP} ${LIBUTIL}

LINKS=	${BINDIR}/ftp ${BINDIR}/pftp \
	${BINDIR}/ftp ${BINDIR}/gate-ftp
MLINKS=	ftp.1 pftp.1 \
	ftp.1 gate-ftp.1

.include <bsd.prog.mk>
