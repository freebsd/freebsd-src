#	$NetBSD: Makefile,v 1.30 2005/02/11 15:13:28 jmc Exp $
#	from: @(#)Makefile	8.2 (Berkeley) 4/3/94

.include <bsd.own.mk>

PROG=	ftp
SRCS=	cmds.c cmdtab.c complete.c domacro.c fetch.c ftp.c main.c \
	progressbar.c ruserpass.c util.c

# Uncomment the following to provide defaults for gate-ftp operation
#
#CPPFLAGS+=-DGATE_SERVER=\"ftp-gw.host\" # -DGATE_PORT=21

.if defined(SMALLPROG)
CPPFLAGS+=-DNO_EDITCOMPLETE -DNO_ABOUT -DNO_AUTH -DNO_HELP -DNO_STATUS
.else
LDADD+=	-ledit -ltermcap
DPADD+=	${LIBEDIT} ${LIBTERMCAP}
.endif

.if (!defined(SMALLPROG) || defined(SMALLPROG_INET6)) && (${USE_INET6} != "no")
CPPFLAGS+= -DINET6
.endif

cmds.o fetch.o: version.h
main.o:	ftp_var.h

.include <bsd.prog.mk>
