# $Id: Makefile,v 1.11 1997/11/07 15:27:12 ache Exp $

NOMAN=YES
CLEANFILES+= ${LOCALES:S/$/.out/g}

LOCALES=        ja_JP.EUC \
		ja_JP.SJIS \
		ko_KR.EUC \
		lt_LN.ASCII \
		lt_LN.ISO_8859-1 \
		lt_LN.ISO_8859-2 \
		ru_SU.CP866 \
		ru_SU.KOI8-R 

LOCALEDIR=	${DESTDIR}/usr/share/locale

.if exists(${.OBJDIR}/../mklocale)
MKLOCALE=${.OBJDIR}/../mklocale
.else
MKLOCALE=${.CURDIR}/../mklocale
.endif

LATIN1LINKS = \
	da_DK de_AT de_CH de_DE en_AU en_CA en_GB en_US es_ES fi_FI \
	fr_BE fr_CA fr_CH fr_FR is_IS it_CH it_IT nl_BE nl_NL no_NO \
	pt_PT sv_SE

LATIN2LINKS = hr_HR pl_PL sl_SI

.SUFFIXES: .src .out

.src.out:
	${MKLOCALE} -o ${.TARGET} ${.IMPSRC}

all: ${LOCALES:S/$/.out/g}

afterinstall:
.for locale in ${LOCALES}
	${INSTALL} ${COPY} -m 644 -o ${BINOWN} -g ${BINGRP} \
		${locale}.out ${LOCALEDIR}/${locale}/LC_CTYPE
.endfor
.for link in ${LATIN1LINKS}
	ln -fs ../lt_LN.ISO_8859-1/LC_CTYPE \
		       ${LOCALEDIR}/${link}.ISO_8859-1/LC_CTYPE
.endfor
.for link in ${LATIN2LINKS}
	ln -fs ../lt_LN.ISO_8859-2/LC_CTYPE \
		${LOCALEDIR}/${link}.ISO_8859-2/LC_CTYPE
.endfor

.include <bsd.prog.mk>
