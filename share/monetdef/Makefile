# $FreeBSD$

LOCALES=	af_ZA.ISO8859-1 \
		am_ET.UTF-8 \
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
		de_AT.ISO8859-1 \
		de_AT.UTF-8 \
		de_CH.ISO8859-1 \
		de_DE.ISO8859-1 \
		de_DE.UTF-8 \
		el_GR.ISO8859-7 \
		el_GR.UTF-8 \
		en_AU.ISO8859-1 \
		en_CA.ISO8859-1 \
		en_GB.ISO8859-1 \
		en_GB.UTF-8 \
		en_IE.UTF-8 \
		en_NZ.ISO8859-1 \
		en_US.ISO8859-1 \
		es_ES.ISO8859-1 \
		es_ES.UTF-8 \
		et_EE.ISO8859-15 \
		fi_FI.ISO8859-1 \
		fi_FI.UTF-8 \
		fr_BE.ISO8859-1 \
		fr_BE.UTF-8 \
		fr_CA.ISO8859-1 \
		fr_FR.ISO8859-1 \
		fr_FR.UTF-8 \
		he_IL.UTF-8 \
		hi_IN.ISCII-DEV \
		hr_HR.ISO8859-2 \
		hu_HU.ISO8859-2 \
		hy_AM.ARMSCII-8 \
		hy_AM.UTF-8 \
		is_IS.ISO8859-1 \
		it_IT.ISO8859-1 \
		it_IT.UTF-8 \
		ja_JP.eucJP \
		ja_JP.UTF-8 \
		kk_KZ.PT154 \
		kk_KZ.UTF-8 \
		ko_KR.eucKR \
		ko_KR.UTF-8 \
		lt_LT.ISO8859-13 \
		lv_LV.ISO8859-13 \
		mn_MN.UTF-8 \
		nl_BE.ISO8859-1 \
		nl_BE.UTF-8 \
		nl_NL.ISO8859-1 \
		nl_NL.UTF-8 \
		no_NO.ISO8859-1 \
		pl_PL.ISO8859-2 \
		pl_PL.UTF-8 \
		pt_BR.ISO8859-1 \
		pt_PT.ISO8859-1 \
		pt_PT.ISO8859-15 \
		pt_PT.UTF-8 \
		ro_RO.ISO8859-2 \
		ru_RU.CP1251 \
		ru_RU.CP866 \
		ru_RU.ISO8859-5 \
		ru_RU.KOI8-R \
		ru_RU.UTF-8 \
		sk_SK.ISO8859-2 \
		sl_SI.ISO8859-2 \
		sr_YU.ISO8859-2 \
		sr_YU.ISO8859-5 \
		sr_YU.UTF-8 \
		sv_SE.ISO8859-1 \
		tr_TR.ISO8859-9 \
		uk_UA.CP1251 \
		uk_UA.ISO8859-5 \
		uk_UA.KOI8-U \
		uk_UA.UTF-8 \
		zh_CN.eucCN \
		zh_CN.GB18030 \
		zh_CN.GB2312 \
		zh_CN.UTF-8 \
		zh_HK.UTF-8 \
		zh_TW.Big5

LOCALEDIR=	${SHAREDIR}/locale

.SUFFIXES: .src .out

.src.out:
	grep -v '^#' < ${.IMPSRC} > ${.TARGET}

FILES=		${LOCALES:S/$/.out/}
FILESNAME=	LC_MONETARY

.for lang in ${LOCALES}
FILESDIR_${lang}.out= ${LOCALEDIR}/${lang}
.endfor

CLEANFILES=	${FILES}

ENCODINGS=	Big5 Big5HKSCS CP949 eucJP eucKR GB2312 GBK \
		ISO8859-1 ISO8859-2 ISO8859-4 ISO8859-7 ISO8859-9 \
		ISO8859-13 ISO8859-15 SJIS US-ASCII UTF-8

Big5_UTF-8=	zh_TW
eucJP_SJIS=	ja_JP
eucKR_CP949=	ko_KR
GB2312_GBK=	zh_CN
ISO8859-1_ISO8859-1= ca_ES:ca_AD ca_ES:ca_FR ca_ES:ca_IT es_ES:eu_ES \
		de_CH:fr_CH de_CH:it_CH no_NO:nb_NO no_NO:nn_NO
ISO8859-1_ISO8859-15= af_ZA ca_AD ca_ES ca_FR ca_IT da_DK de_AT de_CH de_DE \
		en_AU en_CA en_GB en_NZ en_US es_ES \
		fi_FI fr_BE fr_CA fr_CH fr_FR \
		is_IS it_IT it_CH nl_BE nl_NL nb_NO nn_NO no_NO sv_SE
ISO8859-1_US-ASCII= en_AU en_CA en_GB en_NZ en_US
ISO8859-1_UTF-8= af_ZA da_DK de_CH en_AU en_CA en_NZ en_US \
		fr_CA is_IS nb_NO nn_NO no_NO pt_BR sv_SE
ISO8859-2_UTF-8= hr_HR hu_HU ro_RO sk_SK sl_SI
ISO8859-9_UTF-8= tr_TR
ISO8859-13_ISO8859-4= lt_LT
ISO8859-13_UTF-8= lt_LT lv_LV
ISO8859-15_ISO8859-15= es_ES:eu_ES
ISO8859-15_UTF-8= et_EE
UTF-8_Big5HKSCS= zh_HK
UTF-8_UTF-8=	ca_ES:ca_AD ca_ES:ca_FR ca_ES:ca_IT \
		es_ES:eu_ES de_CH:fr_CH de_CH:it_CH

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
