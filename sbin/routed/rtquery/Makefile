# Make `routed` tools for BSD/OS
# $Revision: 1.6 $
# $FreeBSD: src/sbin/routed/rtquery/Makefile,v 1.11 2001/12/04 02:19:57 obrien Exp $

PROG=	rtquery
MAN=	rtquery.8
LDADD+=	-lmd
DPADD+=	${LIBMD}
#COPTS=	-g -DDEBUG -W -Wall -Wcast-align -Wcast-qual -Winline -Wpointer-arith -Wnested-externs -Wwrite-strings -Wunused
WARNS=	0

.include <bsd.prog.mk>
