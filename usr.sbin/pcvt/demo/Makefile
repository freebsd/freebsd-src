PROG=	playvt
SRCS=	playvt.c
DEMOS=	chardemo.vt colors.vt sgr.vt
DEMOS+=	outerlimit.vt twzone.vt cowscene.vt xmas.vt
NOMAN=

all: $(DEMOS) $(PROG)

install: ${DEMOS}
	@${ECHO} "to look at the demos, execute:"
	@${ECHO} "        \"cat <filename>.vt\""
	@${ECHO} "if it is an animation and it runs too fast, try out:"
	@${ECHO} "        \"playvt -f <filename>.vt -d<some-delay-val>\""

.include <bsd.prog.mk>

CLEANFILES+= 	${DEMOS}

# this seems to be the lowest common denominator

chardemo.vt: ${.CURDIR}/$@.gz.uu
	uudecode ${.CURDIR}/$@.gz.uu
	gunzip -f $@.gz
	rm -f $@.gz

colors.vt: ${.CURDIR}/$@.gz.uu
	uudecode ${.CURDIR}/$@.gz.uu
	gunzip -f $@.gz
	rm -f $@.gz

sgr.vt: ${.CURDIR}/$@.gz.uu
	uudecode ${.CURDIR}/$@.gz.uu
	gunzip -f $@.gz
	rm -f $@.gz

cowscene.vt: ${.CURDIR}/$@.gz.uu
	uudecode ${.CURDIR}/$@.gz.uu
	gunzip -f $@.gz
	rm -f $@.gz

xmas.vt: ${.CURDIR}/$@.gz.uu
	uudecode ${.CURDIR}/$@.gz.uu
	gunzip -f $@.gz
	rm -f $@.gz

outerlimit.vt: ${.CURDIR}/$@.gz.uu
	uudecode ${.CURDIR}/$@.gz.uu
	gunzip -f $@.gz
	rm -f $@.gz

twzone.vt: ${.CURDIR}/$@.gz.uu
	uudecode ${.CURDIR}/$@.gz.uu
	gunzip -f $@.gz
	rm -f $@.gz
