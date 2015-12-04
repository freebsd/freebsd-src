# $FreeBSD$

LOCALES=	am_ET.UTF-8 \
		be_BY.CP1131 \
		be_BY.CP1251 \
		be_BY.ISO8859-5 \
		be_BY.UTF-8 \
		bg_BG.CP1251 \
		bg_BG.UTF-8 \
		ca_ES.ISO8859-1 \
		ca_ES.UTF-8 \
		cs_CZ.ISO8859-2 \
		cs_CZ.UTF-8 \
		da_DK.ISO8859-1 \
		da_DK.UTF-8 \
		de_AT.ISO8859-1 \
		de_AT.UTF-8 \
		de_DE.ISO8859-1 \
		de_DE.UTF-8 \
		el_GR.ISO8859-7 \
		el_GR.UTF-8 \
		en_GB.ISO8859-1 \
		en_US.ISO8859-1 \
		es_ES.ISO8859-1 \
		es_ES.UTF-8 \
		et_EE.ISO8859-15 \
		et_EE.UTF-8 \
		eu_ES.ISO8859-1 \
		fi_FI.ISO8859-1 \
		fi_FI.UTF-8 \
		fr_FR.ISO8859-1 \
		fr_FR.UTF-8 \
		he_IL.UTF-8 \
		hi_IN.ISCII-DEV \
		hr_HR.ISO8859-2 \
		hr_HR.UTF-8 \
		hu_HU.ISO8859-2 \
		hu_HU.UTF-8 \
		hy_AM.ARMSCII-8 \
		hy_AM.UTF-8 \
		is_IS.ISO8859-1 \
		is_IS.UTF-8 \
		ja_JP.eucJP \
		ja_JP.SJIS \
		ja_JP.UTF-8 \
		it_IT.ISO8859-1 \
		it_IT.UTF-8 \
		kk_KZ.PT154 \
		kk_KZ.UTF-8 \
		ko_KR.eucKR \
		ko_KR.UTF-8 \
		la_LN.ISO8859-1 \
		lt_LT.ISO8859-4 \
		lt_LT.ISO8859-13 \
		lt_LT.UTF-8 \
		lv_LV.ISO8859-13 \
		lv_LV.UTF-8 \
		mn_MN.UTF-8 \
		nb_NO.ISO8859-1 \
		nb_NO.UTF-8 \
		nl_NL.ISO8859-1 \
		nn_NO.ISO8859-1 \
		nn_NO.UTF-8 \
		pl_PL.ISO8859-2 \
		pl_PL.UTF-8 \
		pt_BR.ISO8859-1 \
		pt_BR.UTF-8 \
		pt_PT.ISO8859-1 \
		pt_PT.UTF-8 \
		ro_RO.ISO8859-2 \
		ro_RO.UTF-8 \
		ru_RU.CP1251 \
		ru_RU.CP866 \
		ru_RU.ISO8859-5 \
		ru_RU.KOI8-R \
		ru_RU.UTF-8 \
		sk_SK.ISO8859-2 \
		sk_SK.UTF-8 \
		sl_SI.ISO8859-2 \
		sl_SI.UTF-8 \
		sr_YU.ISO8859-2 \
		sr_YU.ISO8859-5 \
		sr_YU.UTF-8 \
		sv_SE.ISO8859-1 \
		sv_SE.UTF-8 \
		tr_TR.ISO8859-9 \
		tr_TR.UTF-8 \
		uk_UA.CP1251 \
		uk_UA.ISO8859-5 \
		uk_UA.KOI8-U \
		uk_UA.UTF-8 \
		zh_CN.eucCN \
		zh_CN.GB18030 \
		zh_CN.GB2312 \
		zh_CN.UTF-8 \
		zh_TW.Big5 \
		zh_TW.UTF-8

LOCALEDIR=      ${SHAREDIR}/locale

.SUFFIXES: .src .out

.src.out:
	grep -v '^#' < ${.IMPSRC} > ${.TARGET}

FILES=		${LOCALES:S/$/.out/}
FILESNAME=	LC_TIME

.for locale in ${LOCALES}
FILESDIR_${locale}.out= ${LOCALEDIR}/${locale}
.endfor

CLEANFILES=	${FILES}

ENCODINGS=	Big5HKSCS CP949 eucKR GB2312 GBK \
		ISO8859-1 ISO8859-2 ISO8859-4 ISO8859-15 \
		US-ASCII UTF-8

CA_LINKS=	ca_ES:ca_AD ca_ES:ca_FR ca_ES:ca_IT
DE_LINKS=	de_DE:de_CH
FR_LINKS=	fr_FR:fr_BE fr_FR:fr_CA fr_FR:fr_CH
IT_LINKS=	it_IT:it_CH
NL_LINKS=	nl_NL:nl_BE
NO_LINKS=	nb_NO:no_NO
GB_LINKS=	en_GB:en_AU en_GB:en_CA en_GB:en_NZ

eucKR_CP949=	ko_KR
GB2312_GBK=	zh_CN
ISO8859-1_ISO8859-1= ${CA_LINKS} ${DE_LINKS} ${FR_LINKS} ${GB_LINKS} \
		${IT_LINKS} ${NL_LINKS} ${NO_LINKS} en_US:af_ZA
ISO8859-1_ISO8859-15= ca_ES da_DK de_AT de_DE en_GB en_US es_ES eu_ES \
		fi_FI fr_FR is_IS it_IT la_LN nb_NO nl_NL nn_NO no_NO pt_PT \
		sv_SE \
		${CA_LINKS} ${DE_LINKS} ${FR_LINKS} ${GB_LINKS} ${IT_LINKS} \
		${NL_LINKS}
ISO8859-1_US-ASCII= ${GB_LINKS} en_GB en_US la_LN
ISO8859-1_ISO8859-2= la_LN
ISO8859-1_ISO8859-4= la_LN
ISO8859-1_UTF-8= en_GB en_US eu_ES nl_NL
ISO8859-15_ISO8859-15= en_US:af_ZA
UTF-8_Big5HKSCS= zh_TW:zh_HK
UTF-8_UTF-8=	${CA_LINKS} ${DE_LINKS} ${FR_LINKS} ${GB_LINKS} ${IT_LINKS} \
		${NL_LINKS} ${NO_LINKS} \
		en_GB:en_IE en_US:af_ZA zh_TW:zh_HK

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
