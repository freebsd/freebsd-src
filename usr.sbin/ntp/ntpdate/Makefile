#
# $FreeBSD$
#

.PATH:	${.CURDIR}/../../../contrib/ntp/ntpdate

PROG= ntpdate

SRCS= ntpdate.c version.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

NOMAN= yes

DPADD=	${LIBNTP}
LDADD=	${LIBNTP}

CLEANFILES+= .version version.c

version.c: 
	sh -e ${.CURDIR}/../scripts/mkver ntpdate

.include <bsd.prog.mk>
