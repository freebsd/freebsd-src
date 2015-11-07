# $FreeBSD$
# See https://wiki.freebsd.org/LocaleNewApproach
# Taken from FreeBSD svn [base]/user/edwin/locale/cldr
#
# needs:
# devel/p5-Tie-IxHash
#
# Modified by John Marino to suit DragonFly needs
#

.if !defined(CLDRDIR)
CLDRDIR!=	grep ^cldr etc/unicode.conf | cut -f 2 -d " "
.endif
.if !defined(UNIDATADIR)
UNIDATADIR!=	grep ^unidata etc/unicode.conf | cut -f 2 -d " "
.endif
PASSON=		CLDRDIR="${CLDRDIR}" UNIDATADIR="${UNIDATADIR}"

ETCDIR=		${.CURDIR}/etc

KNOWN=		monetdef numericdef msgdef timedef colldef ctypedef
TYPES?=		${KNOWN}
LOCALE_DESTDIR?=	/tmp/generated-locales/

.if defined(LC)
LC:=	--lc=${LC}
.endif

all:
	cp ${ETCDIR}/common.UTF-8.src ${CLDRDIR}/posix/xx_Comm_US.UTF-8.src
.for t in ${TYPES}
.  if ${KNOWN:M${t}}
	test -d ${t} || mkdir ${t}
	make build-${t}
.  endif
.endfor
	@echo ""
	@find . -name *failed

install:
.for t in ${TYPES}
.  if ${KNOWN:M${t}}
	rm -rf ${.CURDIR}/${t}.draft
	rm -rf ${DESTDIR}/usr/src/share/${t}
	mv ${.CURDIR}/${t} ${DESTDIR}/usr/src/share/
.  endif
.endfor

post-install:
.for t in ${TYPES}
.  if ${KNOWN:M${t}}
	(cd ${DESTDIR}/usr/src/share/${t} && \
		make && make install && make clean)
.  endif
.endfor

.for t in ${TYPES}
build-${t}:
	mkdir -p ${t} ${t}.draft
	perl -I tools tools/cldr2def.pl \
		--cldr=$$(realpath ${CLDRDIR}) \
		--unidata=$$(realpath ${UNIDATADIR}) \
		--etc=$$(realpath ${ETCDIR}) \
		--type=${t} ${LC}
	env ${PASSON} tools/finalize ${t}
.endfor

build-ctypedef:	transfer-rollup

transfer-rollup:
	cp ${ETCDIR}/common.UTF-8.src ${CLDRDIR}/posix/xx_Comm_US.UTF-8.src

rollup:
	perl -I tools tools/utf8-rollup.pl \
		--cldr=$$(realpath ${CLDRDIR}) \
		--etc=$$(realpath ${ETCDIR})

clean:
.for t in ${TYPES}
	rm -rf ${t} ${t}.draft
.endfor

BASE_LOCALES_OF_INTEREST?= \
	af_ZA am_ET ar_AE ar_EG ar_JO ar_MA ar_QA ar_SA \
	be_BY bg_BG ca_AD ca_ES ca_FR ca_IT \
	cs_CZ da_DK de_AT de_CH de_DE el_GR en_AU en_CA \
	en_GB en_HK en_IE en_NZ en_PH en_SG en_US en_ZA \
	es_AR es_CR es_ES es_MX et_EE eu_ES fi_FI fr_BE \
	fr_CA fr_CH fr_FR he_IL hi_IN hr_HR hu_HU hy_AM \
	is_IS it_CH it_IT ja_JP ko_KR lt_LT lv_LV \
	nb_NO nl_BE nl_NL nn_NO pl_PL pt_BR pt_PT ro_RO \
	ru_RU se_FI se_NO sk_SK sl_SI sv_FI sv_SE tr_TR \
	uk_UA \
	kk_Cyrl_KZ mn_Cyrl_MN sr_Cyrl_RS sr_Latn_RS \
	zh_Hans_CN zh_Hant_HK zh_Hant_TW \
	\
	\
	bn_IN gu_IN or_IN ta_IN te_IN kn_IN ml_IN si_LK \
	th_TH lo_LA bo_IN my_MM pa_Guru_IN ka_GE chr_US \
	km_KH shi_Tfng_MA ii_CN vai_Vaii_LR vi_VN

POSIX:
.if exists (${CLDRDIR}/tools/java/cldr.jar)
	mkdir -p ${CLDRDIR}/posix
.  for area in ${BASE_LOCALES_OF_INTEREST}
.    if !exists(${CLDRDIR}/posix/${area}.UTF-8.src)
	java -DCLDR_DIR=${CLDRDIR:Q} -jar ${CLDRDIR}/tools/java/cldr.jar \
		org.unicode.cldr.posix.GeneratePOSIX \
		-d ${CLDRDIR}/posix -m ${area} -c UTF-8
.    endif
.  endfor
.  if !exists(${CLDRDIR}/posix/UTF-8.cm)
	java -DCLDR_DIR=${CLDRDIR:Q} -jar ${CLDRDIR}/tools/java/cldr.jar \
		org.unicode.cldr.posix.GenerateCharmap \
		-d ${CLDRDIR}/posix
.  endif
.else
	@echo "Please install CLDR toolset for the desired release"
	@echo "It should go at ${CLDRDIR}/tools"
.endif

clean-POSIX:
	rm -f {CLDRDIR}/posix/*
