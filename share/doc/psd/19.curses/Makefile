#	From: @(#)Makefile	8.1 (Berkeley) 8/14/93
#	$Id: Makefile,v 1.8 1997/03/03 01:09:51 wosch Exp $

VOLUME=	psd/19.curses
SRCS=	Master
MACROS=	-me
SRCDIR=	${.CURDIR}/../../../../lib/libcurses/PSD.doc

cfiles= ex1.c twinkle1.c
OBJS=	${cfiles:S;.c$;.gr;} intro.2.tbl
files=	intro.0 intro.1 intro.3 intro.4 intro.5 intro.6 \
	macros c_macros fns.doc appen.A
EXTRA=	${files}
CLEANFILES+= ${OBJS} ${files} Master
USE_SOELIM=	yes
USE_SOELIMPP=	yes


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

intro.2.tbl: intro.2
	${TBL} ${SRCDIR}/intro.2 > intro.2.tbl

Master: ${files}
	@for file in ${.TARGET} ${files}; do \
		test -e $$file || ln -s ${SRCDIR}/$${file} . ;\
	done

.include <bsd.doc.mk>
