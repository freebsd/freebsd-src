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

NLSLINKS_fi_FI.ISO8859-1= fi_FI.ISO8859-15
NLSLINKS_fr_FR.ISO8859-1= fr_BE.ISO8859-1 fr_BE.ISO8859-15 \
	fr_CA.ISO8859-1 fr_CA.ISO8859-15 fr_CH.ISO8859-1 fr_CH.ISO8859-15 \
	fr_FR.ISO8859-15
NLSLINKS_de_DE.ISO8859-1= de_AT.ISO8859-1 de_AT.ISO8859-15 de_CH.ISO8859-1 \
	de_CH.ISO8859-15 de_DE.ISO8859-15
NLSLINKS_it_IT.ISO8859-1= it_CH.ISO8859-1 it_CH.ISO8859-15 it_IT.ISO8859-15
NLSLINKS_es_ES.ISO8859-1= es_ES.ISO8859-15

NLSNAME= tcsh

NLSSRCFILES= set[0-9]*

.for catalog in ${CATALOGS}
NLSSRCDIR_${catalog:C/.*://g}.msg:= ${BASESRC}/${catalog:C/:.*//g}
NLS+=${catalog:C/.*://g}.msg
.endfor

.include <bsd.nls.mk>
