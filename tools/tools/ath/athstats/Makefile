# $FreeBSD$

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
