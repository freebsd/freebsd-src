#
# $FreeBSD$
#

.PATH:	${.CURDIR}/../../../contrib/ntp/ntpdate

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

DPADD=	${LIBNTP} ${LIBKVM}
LDADD=	${LIBNTP} ${LIBKVM}

PROG= ntpdate
NOMAN= yes
CLEANFILES+= .version version.c

SRCS= ntpdate.c version.c

version.c: 
	sh -e ${.CURDIR}/../scripts/mkver ntpdate

.include <bsd.prog.mk>
