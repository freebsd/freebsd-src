# $FreeBSD$

MAINTAINER  =  grog@FreeBSD.org
PROG=	vinum
SRCS=	v.c list.c vinumparser.c vinumutil.c vext.h commands.c
MAN8=	vinum.8

CFLAGS+=	-I${.CURDIR}/../../sys -g -Wall -DVINUMDEBUG
DPADD=		${LIBUTIL} ${LIBREADLINE} ${LIBTERMCAP}
LDADD=		-lutil -lreadline -ltermcap -ldevstat

.PATH: ${.CURDIR}/../../sys/dev/vinum

.include <bsd.prog.mk>
