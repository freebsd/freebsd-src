# $FreeBSD: src/sbin/vinum/Makefile,v 1.12 2000/01/03 12:01:26 marcel Exp $

MAINTAINER  =  grog@FreeBSD.org
PROG=	vinum
SRCS=	v.c list.c vinumparser.c vinumutil.c vext.h commands.c
MAN8=	vinum.8

CFLAGS+=	-I${.CURDIR}/../../sys -Wall -DVINUMDEBUG
DPADD=		${LIBUTIL} ${LIBREADLINE} ${LIBTERMCAP}
LDADD=		-lutil -lreadline -ltermcap -ldevstat

.PATH: ${.CURDIR}/../../sys/dev/vinum

.include <bsd.prog.mk>
