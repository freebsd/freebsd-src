# Make `routed` tools for BSD/OS
# $Revision: 1.7 $
# $FreeBSD: src/sbin/routed/rtquery/Makefile,v 1.5.2.2 2000/02/17 04:26:40 peter Exp $

PROG=	rtquery
MAN8=	rtquery.8
LDADD+=	-lmd
DPADD+=	${LIBMD}
#COPTS=	-g -DDEBUG -W -Wall -Wcast-align -Wcast-qual -Winline -Wpointer-arith -Wnested-externs -Wwrite-strings -Wunused

.include <bsd.prog.mk>
