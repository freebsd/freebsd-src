#
# $FreeBSD: src/usr.sbin/ntp/ntpdate/Makefile,v 1.2 2000/01/01 23:57:59 roberto Exp $
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
