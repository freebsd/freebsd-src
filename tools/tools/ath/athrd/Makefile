# $FreeBSD: src/tools/tools/ath/athrd/Makefile,v 1.1.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

.PATH:	${.CURDIR}/../../../../sys/dev/ath/ath_hal

PROG=	athrd

SRCS=	athrd.c ah_regdomain.c opt_ah.h

CLEANFILES+=	opt_ah.h

CFLAGS+= -fno-inline

.include <../Makefile.inc>

MAN=	athrd.1

opt_ah.h:
	echo "#define AH_DEBUG 1" > opt_ah.h
	echo "#define AH_DEBUG_COUNTRY 1" >> opt_ah.h

.include <bsd.prog.mk>
