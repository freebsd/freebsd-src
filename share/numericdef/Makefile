# $FreeBSD$

LOCALES=	af_ZA.ISO8859-1 \
		am_ET.UTF-8 \
		be_BY.CP1251 \
		bg_BG.CP1251 \
		ca_ES.ISO8859-1 \
		cs_CZ.ISO8859-2 \
		da_DK.ISO8859-1 \
		de_CH.ISO8859-1 \
		de_DE.ISO8859-1 \
		el_GR.ISO8859-7 \
		en_US.ISO8859-1 \
		es_ES.ISO8859-1 \
		et_EE.ISO8859-15 \
		eu_ES.ISO8859-1 \
		fi_FI.ISO8859-1 \
		fr_BE.ISO8859-1 \
		fr_FR.ISO8859-1 \
		hi_IN.ISCII-DEV \
		hr_HR.ISO8859-2 \
		hu_HU.ISO8859-2 \
		hy_AM.ARMSCII-8 \
		is_IS.ISO8859-1 \
		it_IT.ISO8859-1 \
		ja_JP.eucJP \
		kk_KZ.PT154 \
		ko_KR.eucKR \
		lt_LT.ISO8859-13 \
		mn_MN.UTF-8 \
		nl_NL.ISO8859-1 \
		no_NO.ISO8859-1 \
		pl_PL.ISO8859-2 \
		pt_BR.ISO8859-1 \
		pt_PT.ISO8859-1 \
		ro_RO.ISO8859-2 \
		ru_RU.KOI8-R \
		sk_SK.ISO8859-2 \
		sl_SI.ISO8859-2 \
		sr_YU.ISO8859-2 \
		sr_YU.ISO8859-5 \
		sv_SE.ISO8859-1 \
		tr_TR.ISO8859-9 \
		uk_UA.KOI8-U \
		zh_CN.eucCN

LOCALEDIR=	/usr/share/locale

.SUFFIXES: .src .out

.src.out:
	grep -v '^#' < ${.IMPSRC} > ${.TARGET}

FILES=		${LOCALES:S/$/.out/}
FILESNAME=	LC_NUMERIC

.for locale in ${LOCALES}
FILESDIR_${locale}.out= ${LOCALEDIR}/${locale}
.endfor

CLEANFILES=	${FILES}

ENCODINGS=	ARMSCII-8 Big5 Big5HKSCS \
		CP1131 CP1251 CP866 CP949 \
		eucCN eucJP eucKR GB18030 GB2312 GBK \
		ISO8859-1 ISO8859-2 ISO8859-4 ISO8859-5 ISO8859-7 ISO8859-9 \
		ISO8859-13 ISO8859-15 \
		KOI8-R KOI8-U PT154 SJIS US-ASCII UTF-8

BE_LINKS=	fr_BE:nl_BE
CA_LINKS=	ca_ES:ca_AD ca_ES:ca_FR ca_ES:ca_IT
DE_LINKS=	de_DE:de_AT
FR_LINKS=	fr_FR:fr_CA fr_FR:fr_CH
GB_LINKS=	en_GB:en_AU en_GB:en_NZ
IT_LINKS=	it_IT:it_CH
NO_LINKS=	no_NO:nb_NO no_NO:nn_NO
US_LINKS=	en_US:en_CA en_US:en_GB

ARMSCII-8_UTF-8= hy_AM
Big5_UTF-8=	zh_TW:zh_HK zh_TW
CP1251_CP1131=	be_BY
CP1251_ISO8859-5= be_BY
CP1251_UTF-8=	be_BY bg_BG
CP866_UTF-8=	ru_RU
eucCN_Big5=	zh_CN:zh_TW
eucCN_Big5HKSCS= zh_CN:zh_HK
eucCN_GB18030=	zh_CN
eucCN_GB2312=	zh_CN
eucCN_GBK=	zh_CN
eucCN_UTF-8=	zh_CN
eucJP_SJIS=	ja_JP
eucJP_UTF-8=	ja_JP
eucKR_CP949=	ko_KR
eucKR_UTF-8=	ko_KR
ISO8859-1_ISO8859-15= af_ZA ca_ES da_DK de_CH de_DE en_US es_ES eu_ES \
		fi_FI fr_BE fr_FR is_IS it_IT nl_NL no_NO pt_PT sv_SE \
		${BE_LINKS} ${CA_LINKS} ${DE_LINKS} ${FR_LINKS} ${GB_LINKS} \
		${IT_LINKS} ${NO_LINKS} ${US_LINKS}
ISO8859-1_ISO8859-1= ${BE_LINKS} ${CA_LINKS} ${DE_LINKS} ${FR_LINKS} \
		${GB_LINKS} ${IT_LINKS} ${NO_LINKS} ${US_LINKS}
ISO8859-1_US-ASCII= en_US ${GB_LINKS} ${US_LINKS}
ISO8859-1_UTF-8= af_ZA ca_ES da_DK de_CH de_DE en_US es_ES eu_ES fi_FI \
		fr_BE fr_FR is_IS it_IT nl_NL no_NO pt_BR pt_PT sv_SE
ISO8859-2_UTF-8= cs_CZ hr_HR hu_HU pl_PL ro_RO sk_SK sl_SI
ISO8859-5_UTF-8= sr_YU uk_UA
ISO8859-5_CP1251= uk_UA
ISO8859-7_UTF-8= el_GR
ISO8859-9_UTF-8= tr_TR
ISO8859-13_ISO8859-4= lt_LT
ISO8859-13_ISO8859-13= lt_LT:lv_LV
ISO8859-13_UTF-8= lt_LT lt_LT:lv_LV
ISO8859-15_UTF-8= et_EE
KOI8-R_CP1251=	ru_RU
KOI8-R_CP866=	ru_RU
KOI8-R_ISO8859-5= ru_RU
KOI8-U_ISO8859-5= uk_UA
PT154_UTF-8=	kk_KZ
US-ASCII_UTF-8=	en_US:he_IL
UTF-8_UTF-8=	${BE_LINKS} ${CA_LINKS} ${DE_LINKS} ${FR_LINKS} ${GB_LINKS} \
		en_GB:en_IE ${IT_LINKS} ${NO_LINKS} ${US_LINKS}

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
