#	from: @(#)Makefile	6.4 (Berkeley) 6/30/90
#	Makefile,v 1.4 1993/07/31 15:33:26 mycroft Exp

VOLUME=	ps1
DOC=	18.curses

SRCS=	Master
MACROS=	-me

CLEANFILES+=${.CURDIR}/win_st.gr ${.CURDIR}/twinkle1.gr \
		${.CURDIR}/twinkle2.gr ${.CURDIR}/life.gr \
		${.CURDIR}/intro.2.tbl ${.CURDIR}/appen.A.tbl

.include <bsd.doc.mk>

${DOC}.${PRINTER}: ${SRCS} win_st.gr twinkle1.gr twinkle2.gr life.gr \
			intro.2.tbl appen.A.tbl
	( cd ${.CURDIR} ; ${ROFF} ${COMPAT} ${SRCS} ) > ${.TARGET}

.SUFFIXES:
.SUFFIXES: .c .gr

#
# this section formats C input source into nice troffable (or nroffable)
# versions.  It uses the capabilites of "vgrind", which sets keywords in
# bold font, and comments in italics.
#

# Don't re-run vgrind unless you want to patch the output files.
VFONT=	/usr/libexec/vfontedpr
.c.gr:
	cd ${.CURDIR} ; ${VFONT} $*.c | grep -v "^'wh" > $*.gr

.VPATH:	${CURDIR}/obj

intro.5: doc.I doc.II doc.III doc.IV
appen.B: win_st.gr
appen.C: twinkle1.gr life.gr twinkle2.gr

intro.2.tbl: intro.2
	cd ${.CURDIR} ; ${TBL} intro.2 > intro.2.tbl

appen.A.tbl: appen.A
	cd ${.CURDIR} ; ${TBL} appen.A > appen.A.tbl
