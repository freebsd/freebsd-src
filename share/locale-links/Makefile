# This creates short versions of locales as symlinks to full versions
# e.g. zh_Hant_TW is a symlink to zh_Hant_TW.UTF-8.

UTF8=	af_ZA am_ET be_BY bg_BG ca_AD ca_ES ca_FR ca_IT \
	cs_CZ da_DK de_AT de_CH de_DE el_GR en_AU en_CA \
	en_GB en_HK en_IE en_NZ en_PH en_SG en_US es_AR \
	es_CR es_ES es_MX et_EE eu_ES fi_FI fr_BE fr_CA \
	fr_CH fr_FR he_IL hi_IN hr_HR hu_HU hy_AM is_IS \
	it_CH it_IT ja_JP ko_KR lt_LT lv_LV nb_NO \
	nl_BE nl_NL nn_NO pl_PL pt_BR pt_PT ro_RO ru_RU \
	se_FI se_NO sk_SK sl_SI sv_FI sv_SE tr_TR uk_UA \
	kk_Cyrl_KZ \
	mn_Cyrl_MN \
	sr_Cyrl_RS \
	sr_Latn_RS \
	zh_Hans_CN \
	zh_Hant_HK \
	zh_Hant_TW

STD1=	af_ZA en_AU en_CA en_HK en_NZ en_SG en_US en_ZA \
	es_AR es_MX fr_CA pt_BR
STD2=	cs_CZ hr_HR hu_HU ro_RO sr_Latn_RS
STD5=	be_BY ru_RU sr_Cyrl_RS
STD15=	ca_AD ca_ES ca_FR ca_IT da_DK de_AT de_CH de_DE \
	en_GB en_IE es_ES et_EE eu_ES fi_FI fr_BE fr_CH \
	fr_FR is_IS it_CH it_IT nb_NO nl_BE nl_NL nn_NO \
	pt_PT sv_FI sv_SE

MANUAL=	lt_LT el_GR lv_LV

LOCALEDIR=	/usr/share/locale

SYMLINKS=	lt_LT.ISO8859-13 ${LOCALEDIR}/lt_LT \
		el_GR.ISO8859-7  ${LOCALEDIR}/el_GR \
		lv_LV.ISO8859-13 ${LOCALEDIR}/lv_LV \
		en_US.ISO8859-1  ${LOCALEDIR}/en_US.ISO-8859-1 \
		ja_JP.eucJP      ${LOCALEDIR}/ja_JP.eucjp

.for symdir in ${STD1}
SYMLINKS+=	${symdir}.ISO8859-1 ${LOCALEDIR}/${symdir}
.endfor
.for symdir in ${STD2}
SYMLINKS+=	${symdir}.ISO8859-2 ${LOCALEDIR}/${symdir}
.endfor
.for symdir in ${STD5}
SYMLINKS+=	${symdir}.ISO8859-5 ${LOCALEDIR}/${symdir}
.endfor
.for symdir in ${STD15}
SYMLINKS+=	${symdir}.ISO8859-15 ${LOCALEDIR}/${symdir}
.endfor
.for symdir in ${UTF8}
.  if ! (${STD1:M${symdir}} || ${STD2:M${symdir}} || \
         ${STD5:M${symdir}} || ${STD15:M${symdir}} || ${MANUAL:M${symdir}})
SYMLINKS+=	${symdir}.UTF-8 ${LOCALEDIR}/${symdir}
.  endif
SYMLINKS+=	${symdir}.UTF-8 ${LOCALEDIR}/${symdir}.UTF8
.endfor

# We need to keep zh_CN.* around as aliases to zh_Hans_CN.* because some
# of the lang catalogs use zh_CN still (e.g. vi), plus people may expect it
# We won't alias zh_Hans_CN or zh_Hans_UTF8 though
.for CN in GB18030 GB2312 GBK UTF-8 eucCN
SYMLINKS+=	zh_Hans_CN.${CN} ${LOCALEDIR}/zh_CN.${CN}
.endfor

.include <bsd.prog.mk>
