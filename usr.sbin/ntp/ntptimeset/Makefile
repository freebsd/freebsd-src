#
# $FreeBSD$
#

.PATH:	${.CURDIR}/../../../contrib/ntp/ntpdate

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

DPADD=	${LIBNTP} ${LIBKVM}
LDADD=	${LIBNTP} ${LIBKVM}

PROG= ntptimeset
NOMAN= yes
CLEANFILES+= .version version.c

SRCS= ntptimeset.c ntptime_config.c version.c

version.c: 
	sh -e ${.CURDIR}/../scripts/mkver ntptimeset

.include <bsd.prog.mk>
