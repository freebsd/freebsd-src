# $FreeBSD$

PROG=	vinum
SRCS=	v.c list.c vinumparser.c vinumutil.c vext.h commands.c
MAN8=	vinum.8

CFLAGS+=	-I${.CURDIR}/../../sys -Wall -DVINUMDEBUG
DPADD=	${LIBREADLINE} ${LIBTERMCAP} ${LIBDEVSTAT}
LDADD=	-lutil -lreadline -ltermcap -ldevstat

.PATH: ${.CURDIR}/../../sys/dev/vinum

.include <bsd.prog.mk>
