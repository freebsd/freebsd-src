#
# $FreeBSD$
#

.PATH:	${.CURDIR}/../../../contrib/ntp/ntptrace

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

DPADD=	${LIBNTP} ${LIBKVM}
LDADD=	${LIBNTP} ${LIBKVM}

PROG= ntptrace
NOMAN= yes
CLEANFILES+= .version version.c

SRCS= 	ntptrace.c version.c

version.c: 
	sh -e ${.CURDIR}/../scripts/mkver ntptrace

.include <bsd.prog.mk>
