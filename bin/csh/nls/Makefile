# $FreeBSD$

BASESRC= ${.CURDIR}/../../../contrib/tcsh/nls

CATALOGS= et:et_EE.ISO8859-15 \
	finnish:fi_FI.ISO8859-1 \
	french:fr_FR.ISO8859-1 \
	german:de_DE.ISO8859-1 \
	greek:el_GR.ISO8859-7 \
	italian:it_IT.ISO8859-1 \
	ja:ja_JP.eucJP \
	russian:ru_RU.KOI8-R \
	spanish:es_ES.ISO8859-1 \
	ukrainian:uk_UA.KOI8-U

CATALOGS_LINKS=	fi_FI.ISO8859-1 fi_FI.ISO8859-15 \
	fr_FR.ISO8859-1 fr_BE.ISO8859-1 \
	fr_FR.ISO8859-1 fr_BE.ISO8859-15 \
	fr_FR.ISO8859-1 fr_CA.ISO8859-1 \
	fr_FR.ISO8859-1 fr_CA.ISO8859-15 \
	fr_FR.ISO8859-1 fr_CH.ISO8859-1 \
	fr_FR.ISO8859-1 fr_CH.ISO8859-15 \
	fr_FR.ISO8859-1 fr_FR.ISO8859-15 \
	de_DE.ISO8859-1 de_AT.ISO8859-1 \
	de_DE.ISO8859-1 de_AT.ISO8859-15 \
	de_DE.ISO8859-1 de_CH.ISO8859-1 \
	de_DE.ISO8859-1 de_CH.ISO8859-15 \
	de_DE.ISO8859-1 de_DE.ISO8859-15 \
	it_IT.ISO8859-1 it_CH.ISO8859-1 \
	it_IT.ISO8859-1 it_CH.ISO8859-15 \
	it_IT.ISO8859-1 it_IT.ISO8859-15 \
	es_ES.ISO8859-1 es_ES.ISO8859-15

NLSNAME= tcsh

.for catalog in ${CATALOGS}
NLSSRCDIR_${catalog:C/.*://g}.msg:= ${BASESRC}/${catalog:C/:.*//g}
NLSSRCFILES_${catalog:C/.*://g}.msg= set[0-9]*
NLS+=${catalog:C/.*://g}.msg
.endfor

afterinstall:
	@set ${CATALOGS_LINKS}; \
	while test $$# -ge 2; do \
		f=$$1/${NLSNAME}.cat; shift; \
		l=$$1/${NLSNAME}.cat; shift; \
		${ECHO} "$$f -> $$l"; \
		ln -fs ../$$f ${DESTDIR}${NLSDIR}/$$l; \
	done; true

.include <bsd.nls.mk>
