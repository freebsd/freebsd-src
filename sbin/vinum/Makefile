# $FreeBSD$

PROG=	vinum
SRCS=	v.c list.c vinumparser.c vinumutil.c vext.h commands.c
MAN=	vinum.8

CFLAGS+=	-I${.CURDIR}/../../sys -Wall -DVINUMDEBUG

# Print __FILE__ and __LINE__ when doing perror()
#CFLAGS+=	-DDEVBUG

DPADD=	${LIBREADLINE} ${LIBTERMCAP} ${LIBDEVSTAT}
LDADD=	-lreadline -ltermcap -ldevstat

.PATH: ${.CURDIR}/../../sys/dev/vinum

.include <bsd.prog.mk>
