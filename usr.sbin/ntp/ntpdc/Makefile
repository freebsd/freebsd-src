#
# $FreeBSD$
#

.PATH:	${.CURDIR}/../../../contrib/ntp/ntpdc

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

DPADD=	${LIBNTP} ${LIBKVM}
LDADD=	${LIBNTP} ${LIBKVM}

PROG= ntpdc
NOMAN= yes
CLEANFILES+= .version version.c

SRCS= 	ntpdc.c ntpdc_ops.c version.c

version.c: 
	sh -e ${.CURDIR}/../scripts/mkver ntpdc

.include <bsd.prog.mk>
