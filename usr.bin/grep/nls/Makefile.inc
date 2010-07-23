# $FreeBSD$
NLSNAME= grep

NLS=	es_ES.ISO8859-1
NLS+=	gl_ES.ISO8859-1
NLS+=	hu_HU.ISO8859-2
NLS+=	pt_BR.ISO8859-1
NLS+=	ru_RU.KOI8-R

NLSSRCDIR= ${.CURDIR}/nls
.for lang in ${NLS}
NLSSRCFILES_${lang}=${lang}.msg
.endfor
