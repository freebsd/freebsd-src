# $FreeBSD$

NOMAN=YES
CLEANFILES+= ${LOCALES:S/$/.out/g}

LOCALES=        de_DE.ISO_8859-1 es_ES.ISO_8859-1 is_IS.ISO_8859-1 \
		lt_LN.ISO_8859-1 \
		ru_SU.CP866 ru_SU.KOI8-R
LOCALEDIR=      ${DESTDIR}/usr/share/locale

.if exists(${.OBJDIR}/../colldef)
COLLDEF=${.OBJDIR}/../colldef
.else
COLLDEF=${.CURDIR}/../colldef
.endif

LATIN1LINKS = \
	da_DK en_AU en_CA en_GB en_US fi_FI \
	fr_BE fr_CA fr_CH fr_FR it_CH it_IT nl_BE nl_NL no_NO \
	pt_PT sv_SE

DELINKS = de_AT de_CH

.SUFFIXES: .src .out

.src.out:
	${COLLDEF} -I ${.CURDIR} -o ${.TARGET} ${.IMPSRC}

all: ${LOCALES:S/$/.out/g}

ru_SU.KOI8-R.out:       map.KOI8-R
ru_SU.CP866.out:        map.CP866
lt_LN.ISO_8859-1.out:   map.ISO_8859-1
es_ES.ISO_8859-1.out:   map.ISO_8859-1
de_DE.ISO_8859-1.out:   map.ISO_8859-1

afterinstall:
	for l in ${LOCALES}; do \
		${INSTALL} -c -m 644 -o ${BINOWN} -g ${BINGRP} \
		$$l.out ${LOCALEDIR}/$$l/LC_COLLATE; \
	done
	for l in ${LATIN1LINKS}; do \
		ln -fs ../lt_LN.ISO_8859-1/LC_COLLATE \
		       ${LOCALEDIR}/$$l.ISO_8859-1/LC_COLLATE; \
	done
	for l in ${DELINKS}; do \
		ln -fs ../de_DE.ISO_8859-1/LC_COLLATE \
		       ${LOCALEDIR}/$$l.ISO_8859-1/LC_COLLATE; \
	done

.include <bsd.prog.mk>
