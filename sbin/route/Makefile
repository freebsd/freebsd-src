#	@(#)Makefile	8.1 (Berkeley) 6/5/93

PROG=	route
MAN8=	route.8
SRCS=	route.c keywords.h # ccitt_addr.h
CFLAGS+=-I. -Wall
CLEANFILES+=keywords.h
BINOWN=	root
BINMODE=4555

keywords.h: keywords
	sed -e '/^#/d' -e '/^$$/d' ${.CURDIR}/keywords > _keywords.tmp
	tr a-z A-Z < _keywords.tmp | paste _keywords.tmp - | \
	    awk '{ \
		if (NF > 1) \
			printf "#define\tK_%s\t%d\n\t{\"%s\", K_%s},\n", \
			    $$2, NR, $$1, $$2 }' \
	    > ${.TARGET}
	rm -f _keywords.tmp

./keywords.h:	keywords.h

.include <bsd.prog.mk>

route .depend lint tags: keywords.h
