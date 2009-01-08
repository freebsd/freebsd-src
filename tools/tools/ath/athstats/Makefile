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
	touch ah_osdep.h
