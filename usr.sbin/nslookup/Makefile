# $FreeBSD: src/usr.sbin/nslookup/Makefile,v 1.10.2.2 2002/02/18 16:24:26 ru Exp $

USE_LIBBIND=	yes
.include "${.CURDIR}/../named/Makefile.inc"

.PATH:	${BIND_DIR}/bin/nslookup
.PATH:	${BIND_DIR}/doc/man

PROG=	nslookup
SRCS=	main.c getinfo.c debug.c send.c skip.c list.c subr.c commands.l
MAN=	nslookup.8

CFLAGS+=-D_PATH_HELPFILE=\"${DESTHELP}/nslookup.help\"
LDADD+=	-ll -ledit -ltermcap
DPADD+=	${LIBL} ${LIBEDIT} ${LIBTERMCAP}

FILES=	nslookup.help
FILESDIR=	${DESTHELP}

.include <bsd.prog.mk>
