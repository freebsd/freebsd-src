# $FreeBSD$

LOCALES=	UTF-8 \
		am_ET.UTF-8 \
		be_BY.CP1131 \
		bg_BG.CP1251 \
		el_GR.ISO8859-7 \
		hi_IN.ISCII-DEV \
		hy_AM.ARMSCII-8 \
		ja_JP.SJIS \
		ja_JP.eucJP \
		kk_KZ.PT154 \
		ko_KR.CP949 \
		ko_KR.eucKR \
		la_LN.ISO8859-1 \
		la_LN.ISO8859-13 \
		la_LN.ISO8859-15 \
		la_LN.ISO8859-2 \
		la_LN.ISO8859-4 \
		la_LN.US-ASCII \
		ru_RU.CP866 \
		ru_RU.ISO8859-5 \
		ru_RU.KOI8-R \
		tr_TR.ISO8859-9 \
		uk_UA.KOI8-U \
		zh_CN.eucCN \
		zh_CN.GB18030 \
		zh_CN.GB2312 \
		zh_CN.GBK \
		zh_HK.Big5HKSCS \
		zh_TW.Big5

LOCALEDIR=	${SHAREDIR}/locale

.SUFFIXES: .src .out

.src.out:
	mklocale -o ${.TARGET} ${.IMPSRC}

FILES=		${LOCALES:S/$/.out/}
FILESNAME=	LC_CTYPE

.for locale in ${LOCALES}
FILESDIR_${locale}.out= ${LOCALEDIR}/${locale}
.endfor

CLEANFILES=	${FILES}

ENCODINGS=      CP1251 ISO8859-1 ISO8859-2 ISO8859-4 ISO8859-5 \
		ISO8859-13 ISO8859-15 US-ASCII UTF-8

ASCIILINKS=	en_AU en_CA en_GB en_NZ en_US
US-ASCII_US-ASCII= ${ASCIILINKS:C/^/la_LN:/}

LATIN1XLINKS=	af_ZA ca_AD ca_ES ca_FR ca_IT da_DK de_AT de_CH de_DE \
		en_AU en_CA en_GB en_NZ en_US es_ES eu_ES \
		fi_FI fr_BE fr_CA fr_CH fr_FR is_IS it_CH \
		it_IT nb_NO nl_BE nl_NL nn_NO no_NO pt_PT sv_SE
LATIN1LINKS=	${LATIN1XLINKS} pt_BR
ISO8859-1_ISO8859-1= ${LATIN1LINKS:C/^/la_LN:/}

LATIN15LINKS=	${LATIN1XLINKS} et_EE
ISO8859-15_ISO8859-15= ${LATIN15LINKS:C/^/la_LN:/}

LATIN2LINKS=	cs_CZ hr_HR hu_HU pl_PL sl_SI sr_YU ro_RO sk_SK
ISO8859-2_ISO8859-2= ${LATIN2LINKS:C/^/la_LN:/}

LATIN4LINKS=	lt_LT
ISO8859-4_ISO8859-4= ${LATIN4LINKS:C/^/la_LN:/}

LATIN5LINKS=	sr_YU uk_UA be_BY
ISO8859-5_ISO8859-5= ${LATIN5LINKS:C/^/ru_RU:/}

LATIN13LINKS=	lt_LT lv_LV
ISO8859-13_ISO8859-13= ${LATIN13LINKS:C/^/la_LN:/}

CP1251LINKS=    ru_RU be_BY uk_UA
CP1251_CP1251=	${CP1251LINKS:C/^/bg_BG:/}

UTF8LINKS=	af_ZA be_BY bg_BG ca_AD ca_ES ca_FR ca_IT cs_CZ \
		da_DK de_AT de_CH de_DE el_GR \
		en_AU en_CA en_GB en_IE en_NZ en_US es_ES et_EE eu_ES fi_FI \
		fr_BE \
		fr_CA fr_CH fr_FR he_IL hr_HR hu_HU hy_AM is_IS it_CH it_IT \
		ja_JP kk_KZ ko_KR lt_LT lv_LV mn_MN \
		nb_NO nl_BE nl_NL nn_NO no_NO pl_PL pt_BR pt_PT \
		ro_RO ru_RU sk_SK sl_SI sr_YU sv_SE tr_TR uk_UA zh_CN zh_HK \
		zh_TW
UTF-8_UTF-8=	${UTF8LINKS:C/^/:/}


SYMLINKS=
.for enc1 in ${ENCODINGS}
.for enc2 in ${ENCODINGS}
.for lang_terr in ${${enc1}_${enc2}}
SYMLINKS+=	../${lang_terr:C/:.*$//:C/.+$/&./}${enc1}/${FILESNAME} \
		${LOCALEDIR}/${lang_terr:C/^.*://}.${enc2}
.endfor
.endfor
.endfor

.include <bsd.prog.mk>
