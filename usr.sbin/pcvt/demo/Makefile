# $FreeBSD$

PROG=	playvt
NOMAN=
SRCS=	playvt.c

DEMOS=	chardemo.vt colors.vt sgr.vt
DEMOS+=	outerlimit.vt twzone.vt cowscene.vt xmas.vt
CLEANFILES+= 	${DEMOS}

all: $(DEMOS) $(PROG)

install:
	@${ECHO} "to look at the demos, execute:"
	@${ECHO} "        \"cat <filename>.vt\""
	@${ECHO} "if it is an animation and it runs too fast, try out:"
	@${ECHO} "        \"playvt -f <filename>.vt -d<some-delay-val>\""

.include <bsd.prog.mk>

# this seems to be the lowest common denominator

chardemo.vt: ${.CURDIR}/${.TARGET}.gz.uu
	uudecode ${.CURDIR}/${.TARGET}.gz.uu
	gunzip -f ${.TARGET}.gz
	rm -f ${.TARGET}.gz

colors.vt: ${.CURDIR}/${.TARGET}.gz.uu
	uudecode ${.CURDIR}/${.TARGET}.gz.uu
	gunzip -f ${.TARGET}.gz
	rm -f ${.TARGET}.gz

sgr.vt: ${.CURDIR}/${.TARGET}.gz.uu
	uudecode ${.CURDIR}/${.TARGET}.gz.uu
	gunzip -f ${.TARGET}.gz
	rm -f ${.TARGET}.gz

cowscene.vt: ${.CURDIR}/${.TARGET}.gz.uu
	uudecode ${.CURDIR}/${.TARGET}.gz.uu
	gunzip -f ${.TARGET}.gz
	rm -f ${.TARGET}.gz

xmas.vt: ${.CURDIR}/${.TARGET}.gz.uu
	uudecode ${.CURDIR}/${.TARGET}.gz.uu
	gunzip -f ${.TARGET}.gz
	rm -f ${.TARGET}.gz

outerlimit.vt: ${.CURDIR}/${.TARGET}.gz.uu
	uudecode ${.CURDIR}/${.TARGET}.gz.uu
	gunzip -f ${.TARGET}.gz
	rm -f ${.TARGET}.gz

twzone.vt: ${.CURDIR}/${.TARGET}.gz.uu
	uudecode ${.CURDIR}/${.TARGET}.gz.uu
	gunzip -f ${.TARGET}.gz
	rm -f ${.TARGET}.gz
