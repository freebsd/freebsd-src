# $FreeBSD$

NOMAN=YES
CLEANFILES+= ${LOCALES:S/$/.out/g}

LOCALES=        ja_JP.EUC \
		ja_JP.SJIS \
		ko_KR.EUC \
		la_LN.ASCII \
		la_LN.ISO_8859-1 \
		la_LN.ISO_8859-2 \
		la_LN.ISO_8859-4 \
		la_LN.DIS_8859-15 \
		ru_SU.CP866 \
		ru_SU.ISO_8859-5 \
		ru_SU.KOI8-R \
		uk_UA.KOI8-U \
		zh_CN.EUC \
		zh_TW.Big5

LOCALEDIR=	${DESTDIR}/usr/share/locale

LATIN1LINKS = \
	da_DK de_AT de_CH de_DE en_AU en_CA en_GB en_US es_ES fi_FI \
	fr_BE fr_CA fr_CH fr_FR is_IS it_CH it_IT nl_BE nl_NL no_NO \
	pt_PT sv_SE

LATIN2LINKS = cs_CZ hr_HR hu_HU pl_PL sl_SI

LATIN4LINKS = lt_LT

.SUFFIXES: .src .out

.src.out:
	mklocale -o ${.TARGET} ${.IMPSRC}

all: ${LOCALES:S/$/.out/g}

afterinstall:
.for locale in ${LOCALES}
	${INSTALL} ${COPY} -m 644 -o ${BINOWN} -g ${BINGRP} \
		${locale}.out ${LOCALEDIR}/${locale}/LC_CTYPE
.endfor
.for link in ${LATIN1LINKS}
	ln -fs ../la_LN.ISO_8859-1/LC_CTYPE \
		       ${LOCALEDIR}/${link}.ISO_8859-1/LC_CTYPE
.endfor
.for link in ${LATIN1LINKS}
	ln -fs ../la_LN.DIS_8859-15/LC_CTYPE \
		       ${LOCALEDIR}/${link}.DIS_8859-15/LC_CTYPE
.endfor
.for link in ${LATIN2LINKS}
	ln -fs ../la_LN.ISO_8859-2/LC_CTYPE \
		${LOCALEDIR}/${link}.ISO_8859-2/LC_CTYPE
.endfor
.for link in ${LATIN4LINKS}
	ln -fs ../la_LN.ISO_8859-4/LC_CTYPE \
		${LOCALEDIR}/${link}.ISO_8859-4/LC_CTYPE
.endfor
	ln -fs ../ja_JP.SJIS/LC_CTYPE \
		${LOCALEDIR}/ja_JP.Shift_JIS/LC_CTYPE

.include <bsd.prog.mk>
