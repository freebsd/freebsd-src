#	From: @(#)Makefile	8.1 (Berkeley) 8/14/93
#	$Id$

VOLUME=	psd/19.curses
SRCS=	Master
MACROS=	-me
LSRCDIR=	${.CURDIR}/../../../../lib/libcurses/PSD.doc
.PATH:	${LSRCDIR}

CLEANFILES+=win_st.gr twinkle1.gr twinkle2.gr life.gr intro.2.tbl appen.A.tbl \
	ex1.gr ex2.gr ${DOC}.${PRINTER}+

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
	${VFONT} ${.IMPSRC} | grep -v "^'wh" > ${.TARGET}

.MAIN:	all

Master: twinkle1.gr ex1.gr ex2.gr fns.doc intro.5 intro.2.tbl intro.0 intro.1 \
	intro.3 intro.4 intro.6 macros c_macros appen.A
	for a in Master ${.ALLSRC}; do \
		test -e $$a || ln -s ${LSRCDIR}/$$a .; \
	done

intro.2.tbl: intro.2
	${TBL} ${LSRCDIR}/intro.2 > intro.2.tbl

#!!!bogus!!!  If .ALLSRC worked right, this BS wouldn't be necessary.
SRCDIR=	${.OBJDIR}

.include <bsd.doc.mk>





