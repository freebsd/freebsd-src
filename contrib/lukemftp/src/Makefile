#	$NetBSD: Makefile,v 1.26 2003/01/21 16:08:06 jhawk Exp $
#	from: @(#)Makefile	8.2 (Berkeley) 4/3/94

PROG=	ftp
SRCS=	cmds.c cmdtab.c complete.c domacro.c fetch.c ftp.c main.c \
	progressbar.c ruserpass.c util.c

# Uncomment the following to provide defaults for gate-ftp operation
#
#CPPFLAGS+=-DGATE_SERVER=\"ftp-gw.host\" # -DGATE_PORT=21

.if defined(SMALLPROG)
CPPFLAGS+=-DNO_EDITCOMPLETE -DNO_ABOUT
.else
LDADD+=	-ledit -ltermcap
DPADD+=	${LIBEDIT} ${LIBTERMCAP}
.endif

.if !defined(SMALLPROG) || defined(SMALLPROG_INET6)
CPPFLAGS+= -DINET6
.endif

cmds.o fetch.o: version.h
main.o:	ftp_var.h

.include <bsd.prog.mk>
