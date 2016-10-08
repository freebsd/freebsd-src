SRCS=	pm.in \
	andoc.in \
	bib.in \
	doc.in \
	e.in \
	g.in \
	m.in \
	s.in

IMACS=	an \
	doc-ditroff

.SUFFIXES: .in
.in:
	sed 's:@MACDIR@:${MACDIR}:; s:@LIBDIR@:${LIBDIR}:' ${.IMPSRC} >${.TARGET}

.include <bsd.prog.mk>
