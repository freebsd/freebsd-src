# $FreeBSD$

LOCALES=	bg_BG.CP1251 \
		be_BY.CP1131 \
		be_BY.CP1251 \
		be_BY.ISO8859-5 \
		ca_ES.ISO8859-1 \
		ca_ES.ISO8859-15 \
		cs_CZ.ISO8859-2 \
		de_DE.ISO8859-1 \
		de_DE.ISO8859-15 \
		el_GR.ISO8859-7 \
		es_ES.ISO8859-1 \
		es_ES.ISO8859-15 \
		et_EE.ISO8859-15 \
		hi_IN.ISCII-DEV \
		hr_HR.ISO8859-2 \
		hy_AM.ARMSCII-8 \
		is_IS.ISO8859-1 \
		is_IS.ISO8859-15 \
		kk_KZ.PT154 \
		la_LN.ISO8859-1 \
		la_LN.ISO8859-13 \
		la_LN.ISO8859-15 \
		la_LN.ISO8859-2 \
		la_LN.ISO8859-4 \
		la_LN.US-ASCII \
		no_NO.ISO8859-1 \
		no_NO.ISO8859-15 \
		lt_LT.ISO8859-4 \
		pl_PL.ISO8859-2 \
		ru_RU.CP1251 \
		ru_RU.CP866 \
		ru_RU.ISO8859-5 \
		ru_RU.KOI8-R \
		sl_SI.ISO8859-2 \
		sr_YU.ISO8859-5 \
		sv_SE.ISO8859-1 \
		sv_SE.ISO8859-15 \
		tr_TR.ISO8859-9 \
		uk_UA.CP1251 \
		uk_UA.ISO8859-5 \
		uk_UA.KOI8-U

LOCALEDIR=	${SHAREDIR}/locale

.SUFFIXES: .src .out

.src.out:
	colldef -I ${.CURDIR} -o ${.TARGET} ${.IMPSRC}

FILES=		${LOCALES:S/$/.out/}
FILESNAME=	LC_COLLATE

.for locale in ${LOCALES}
FILESDIR_${locale}.out= ${LOCALEDIR}/${locale}
.if exists(${.CURDIR}/map.${locale:E})
${locale}.out: map.${locale:E}
.endif
.endfor

CLEANFILES=	${FILES}

ENCODINGS=	Big5 Big5HKSCS CP949 eucCN eucJP eucKR GB18030 GB2312 GBK \
		ISO8859-1 ISO8859-2 ISO8859-13 ISO8859-15 SJIS US-ASCII UTF-8

ISO8859-1_Big5=	is_IS:zh_TW
ISO8859-1_ISO8859-1= ${ISO8859-15_ISO8859-15} pt_PT:pt_BR

LATIN1LINKS=	af_ZA da_DK en_AU en_CA en_GB en_NZ en_US eu_ES fi_FI \
		fr_BE fr_CA fr_CH fr_FR it_CH it_IT nl_BE nl_NL pt_PT
ISO8859-15_ISO8859-15= \
	ca_ES:ca_AD ca_ES:ca_FR ca_ES:ca_IT \
	de_DE:de_AT de_DE:de_CH \
	no_NO:nb_NO no_NO:nn_NO \
	${LATIN1LINKS:C/^/la_LN:/}

LATIN2LINKS=    hu_HU ro_RO sr_YU
ISO8859-2_ISO8859-2= ${LATIN2LINKS:C/^/la_LN:/} cs_CZ:sk_SK

LATIN13LINKS=	lt_LT lv_LV
ISO8859-13_ISO8859-13= ${LATIN13LINKS:C/^/la_LN:/}

US-ASCII_Big5HKSCS= la_LN:zh_HK
US-ASCII_CP949=	la_LN:ko_KR
US-ASCII_eucCN=	la_LN:zh_CN
US-ASCII_eucJP=	la_LN:ja_JP
US-ASCII_eucKR=	la_LN:ko_KR
US-ASCII_GB18030= la_LN:zh_CN
US-ASCII_GB2312= la_LN:zh_CN
US-ASCII_GBK=	la_LN:zh_CN
US-ASCII_SJIS=	la_LN:ja_JP

ASCIILINKS=	en_AU en_CA en_GB en_NZ en_US
US-ASCII_US-ASCII= ${ASCIILINKS:C/^/la_LN:/}

UTF8LINKS=	af_ZA am_ET be_BY bg_BG ca_AD ca_ES ca_FR ca_IT cs_CZ \
		da_DK de_AT de_CH de_DE \
		el_GR en_AU en_CA en_GB en_IE en_NZ en_US es_ES et_EE eu_ES \
		fi_FI fr_BE fr_CA fr_CH fr_FR he_IL hr_HR hu_HU hy_AM \
		is_IS it_CH it_IT ja_JP kk_KZ ko_KR lt_LT lv_LV mn_MN \
		nb_NO nl_BE nl_NL nn_NO no_NO pl_PL pt_BR pt_PT \
		ro_RO ru_RU sk_SK sl_SI sr_YU sv_SE tr_TR uk_UA zh_CN zh_HK \
		zh_TW
US-ASCII_UTF-8=	${UTF8LINKS:C/^/la_LN:/}

SYMLINKS=
.for enc1 in ${ENCODINGS}
.for enc2 in ${ENCODINGS}
.for lang_terr in ${${enc1}_${enc2}}
SYMLINKS+=	../${lang_terr:C/:.*$//}.${enc1}/${FILESNAME} \
		${LOCALEDIR}/${lang_terr:C/^.*://}.${enc2}
.endfor
.endfor
.endfor

.include <bsd.prog.mk>
