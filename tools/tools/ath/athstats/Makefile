# $FreeBSD$

PROG=	athstats

SRCS=	main.c statfoo.c athstats.c

.include <bsd.prog.mk>

SRCDIR=	${.CURDIR}/../../../..

CLEANFILES+= opt_ah.h ah_osdep.h

CFLAGS+=-DATH_SUPPORT_ANI
CFLAGS+=-DATH_SUPPORT_TDMA

CFLAGS+=-I${.CURDIR}
CFLAGS+=-I${SRCDIR}/sys/net80211

.include <../Makefile.inc>

athstats.o: opt_ah.h ah_osdep.h

opt_ah.h:
	touch opt_ah.h
ah_osdep.h:
	echo 'typedef void *HAL_SOFTC;' >ah_osdep.h
	echo 'typedef int HAL_BUS_TAG;' >>ah_osdep.h
	echo 'typedef void *HAL_BUS_HANDLE;' >>ah_osdep.h
