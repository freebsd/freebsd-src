DEV=		dvi
FONTFILES=	R I B BI CW MI S EX H HI HB SA SB DESC $(DEVGENFILES)
DEVGENFILES=	generate/CompileFonts generate/Makefile generate/msam.map\
		generate/msbm.map generate/texb.map generate/texex.map \
		generate/texi.map generate/texmi.map generate/texr.map \
		generate/texsy.map generate/textt.map generate/cork.map

CLEANFILES+=	DESC

DEVFILES=	$(FONTFILES)

.include "../Makefile.inc"

DESC:	DESC.in
	cat ${DISTDIR}/DESC.in >DESC
	test -z '${DVIPRINT}' || echo print '${DVIPRINT}' >>DESC

.include "../Makefile.dev"
