#
# $FreeBSD: src/tools/regression/acct/Makefile,v 1.1.12.1 2010/02/10 00:26:20 kensmith Exp $
#
# "make" will compile what is needed and run the regression tests.
#
PROG=	pack
NO_MAN=

all: regress

pack: pack.c convert.c

convert.c: ../../../sys/kern/kern_acct.c
	sed -n '/FLOAT_CONVERSION_START/,/FLOAT_CONVERSION_END/p' $? >$@

regress: pack regress.t
	./regress.t

clean:
	rm -f $(PROG) convert.c

.include <bsd.prog.mk>
