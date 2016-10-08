SRCS=	pm.in \
	andoc.in \
	bib.in \
	doc.in \
	e.in \
	g.in \
	m.in \
	s.in

GENFILES=	an \
		bib \
		doc \
		g \
		pm \
		andoc \
		doc-ditroff \
		e \
		m \
		s
CLEANFILES+=	${GENFILES}

FILES=	acm.me \
	deltext.me \
	eqn.me \
	local.me \
	ms.acc \
	ms.tbl\
	null.me \
	refer.me \
	taa \
	v \
	an-ext \
	doc-common \
	float.me \
	mmn \
	ms.cov \
	ms.ths \
	padj \
	safe \
	tbl.me \
	vgrind \
	chars.me \
	doc-nroff \
	footnote.me \
	mmt \
	ms.eqn \
	ms.toc \
	pictures \
	sh.me \
	thesis.me \
	color \
	doc-syms \
	index.me \
	ms.ref \
	naa \
	ptx \
	srefs \
	tmac.gchar \
	${GENFILES}

FILESDIR=	${MACDIR}

MAN=	mcolor.7 mpictures.7 man.7

IMACS=	an \
	doc-ditroff

.SUFFIXES: .in
.in:
	sed 's:@MACDIR@:${MACDIR}:; s:@LIBDIR@:${LIBDIR}:' ${.IMPSRC} >${.TARGET}

.include <bsd.prog.mk>
.PATH: ${DOCTOOLSDIR}/troff/troff.d/tmac.d
