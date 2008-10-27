# $FreeBSD$

PROG=	athstats
BINDIR=	/usr/local/bin
NO_MAN=

SRCS=	main.c statfoo.c athstats.c

.include <bsd.prog.mk>

SRCDIR=	${.CURDIR}/../../../..

CLEANFILES+= opt_ah.h ah_osdep.h

CFLAGS+=-DATH_SUPPORT_ANI

CFLAGS+=-I${.CURDIR}
CFLAGS+=-I${SRCDIR}/sys/contrib/dev/ath
CFLAGS+=-I${SRCDIR}/sys/net80211
CFLAGS+=-I${SRCDIR}/sys/dev/ath

athstats.o: opt_ah.h ah_osdep.h

opt_ah.h:
	touch opt_ah.h
ah_osdep.h:
	touch ah_osdep.h
