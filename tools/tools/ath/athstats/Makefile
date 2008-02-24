# $FreeBSD: src/tools/tools/ath/athstats/Makefile,v 1.4 2007/02/02 02:39:56 sam Exp $

PROG=	athstats
BINDIR=	/usr/local/bin
NO_MAN=

SRCS=	main.c statfoo.c athstats.c

.include <bsd.prog.mk>

CFLAGS+= -I.
CLEANFILES+= opt_ah.h

athstats.o: opt_ah.h

opt_ah.h:
	touch opt_ah.h
