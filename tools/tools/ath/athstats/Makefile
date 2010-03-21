# $FreeBSD: src/tools/tools/ath/athstats/Makefile,v 1.4.2.1.6.1 2010/02/10 00:26:20 kensmith Exp $

PROG=	athstats
BINDIR=	/usr/local/bin
NO_MAN=

SRCS=	main.c statfoo.c athstats.c

.include <bsd.prog.mk>

CFLAGS+= -I.
CLEANFILES+= opt_ah.h ah_osdep.h

CFLAGS+=-I../../../../sys/contrib/dev/ath
CFLAGS+=-I../../../../sys/net80211
CFLAGS+=-I../../../../sys/dev/ath

athstats.o: opt_ah.h ah_osdep.h

opt_ah.h:
	touch opt_ah.h
ah_osdep.h:
	touch ah_osdep.h
