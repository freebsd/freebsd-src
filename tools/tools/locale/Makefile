# $FreeBSD$
# See https://wiki.freebsd.org/LocaleNewApproach
# Taken from FreeBSD svn [base]/user/edwin/locale/cldr
#
# needs:
# devel/p5-Tie-IxHash
#
# Modified by John Marino to suit DragonFly needs
#
.if ${.CURDIR} == ${.OBJDIR}
.error Do make obj first.
.endif

LOCALESRCDIR?=	${SRCTOP}/share
TMPDIR?=	/tmp

BASEDIR=	${.CURDIR}
ETCDIR=		${BASEDIR}/etc
TOOLSDIR=	${BASEDIR}/tools
PATCHDIR=	${BASEDIR}/patch
UNIDIR=		${.OBJDIR:tA}/unicode

PKGS=	openjdk8 \
	apache-ant \
	p5-XML-Parser \
	p5-Tie-IxHash \
	p5-Text-Iconv
tools-test:
	pkg info -e ${PKGS}
	@echo tools ok.

KNOWN=		monetdef numericdef msgdef colldef ctypedef # timedef
TYPES?=		${KNOWN}

COLLATION_SPECIAL?= \
	cs_CZ ISO8859-2 \
	da_DK ISO8859-1 \
	da_DK ISO8859-15 \
	hr_HR ISO8859-2 \
	hu_HU ISO8859-2 \
	nb_NO ISO8859-1 \
	nb_NO ISO8859-15 \
	sk_SK ISO8859-2 \
	sr_Latn_RS ISO8859-2 \
	sr_Cyrl_RS ISO8859-5 \
	zh_Hans_CN GB2312 \
	zh_Hans_CN eucCN \
	zh_Hant_TW Big5 \
	zh_Hans_CN GB18030 \
	zh_Hans_CN GBK \
	ja_JP eucJP \
	nn_NO ISO8859-15 \
	nn_NO ISO8859-1

.for area enc in ${COLLATION_SPECIAL}
COLLATIONS_SPECIAL_ENV+=	${area}.${enc}
.endfor
SETENV=	env -i \
	PATH="${PATH}" \
	TMPDIR="${TMPDIR}" \
	COLLATIONS_SPECIAL="${COLLATIONS_SPECIAL_ENV}" \
	UNIDIR="${UNIDIR}" \
	BASEDIR="${BASEDIR}" \
	TOOLSDIR="${TOOLSDIR}" \
	ETCDIR="${ETCDIR}"

all: posix build afterbuild
.ORDER: posix build afterbuild

afterbuild: build
	@echo ""
	@find . -name *failed

.for t in ${TYPES}
.  if ${KNOWN:M${t}}
build: build-${t}
.ORDER: build-${t} afterbuild
.  endif
.endfor

diff:
.for t in ${TYPES}
.  if ${KNOWN:M${t}}
diff: diff-${t}
diff-${t}:
	-/usr/bin/diff -ruN -x Makefile -x Makefile.depend \
	    ${LOCALESRCDIR}/${t} ${t}
.  endif
.endfor

install:
.for t in ${TYPES}
.  if ${KNOWN:M${t}}
install: install-${t}
install-${t}:
	cd ${LOCALESRCDIR}/${t} && \
	    rm -f Makefile *.src && \
	    cd ${.OBJDIR} && \
	    install -m 644 ${t}/* ${LOCALESRCDIR}/${t}
.  endif
.endfor

post-install:
.for t in ${TYPES}
.  if ${KNOWN:M${t}}
	cd ${LOCALSRCDIR}/${t} && \
	    make && make install && make clean
.  endif
.endfor

.for t in ${TYPES}
CLEANDIRS+=	${t} ${t}.draft
${t}:
	mkdir -p ${t} ${t}.draft && \
	perl -I ${TOOLSDIR} ${TOOLSDIR}/cldr2def.pl \
		--unidir=${UNIDIR:tA} \
		--etc=${ETCDIR:tA} \
		--type=${t}

build-${t}: ${t}
	${SETENV} OUTBASEDIR="${.OBJDIR}/${t}" ${TOOLSDIR}/finalize ${t}
.endfor

static-colldef: colldef
build-colldef:	static-colldef

static-colldef:
.for area enc in ${COLLATION_SPECIAL}
	awk -f ${TOOLSDIR}/extract-colldef.awk \
	    posix/${area}.${enc}.src > colldef.draft/${area}.${enc}.src
.endfor

BASE_LOCALES_OF_INTEREST?= \
	af_ZA am_ET ar_AE ar_EG ar_JO ar_MA ar_QA ar_SA \
	be_BY bg_BG ca_AD ca_ES ca_FR ca_IT \
	cs_CZ da_DK de_AT de_CH de_DE el_GR en_AU en_CA \
	en_GB en_HK en_IE en_NZ en_PH en_SG en_US en_ZA \
	es_AR es_CR es_ES es_MX et_EE eu_ES fi_FI fr_BE \
	fr_CA fr_CH fr_FR ga_IE he_IL hi_IN hr_HR hu_HU hy_AM \
	is_IS it_CH it_IT ja_JP ko_KR lt_LT lv_LV \
	nb_NO nl_BE nl_NL nn_NO pl_PL pt_BR pt_PT ro_RO \
	ru_RU se_FI se_NO sk_SK sl_SI sv_FI sv_SE tr_TR \
	uk_UA \
	kk_KZ mn_MN sr_Cyrl_RS sr_Latn_RS \
	zh_Hans_CN zh_Hant_HK zh_Hant_TW \
	bn_IN gu_IN or_IN ta_IN te_IN kn_IN ml_IN si_LK \
	th_TH lo_LA bo_IN my_MM pa_Guru_IN ka_GE chr_US \
	km_KH shi_Tfng_MA ii_CN vai_Vaii_LR vi_VN

ENCODINGS=	Big5 \
		CP1251 \
		CP866 \
		CP949 \
		eucCN \
		eucJP \
		eucKR \
		GB18030 \
		GB2312 \
		GBK \
		ISO8859-1 \
		ISO8859-13 \
		ISO8859-15 \
		ISO8859-2 \
		ISO8859-5 \
		ISO8859-7 \
		ISO8859-9 \
		KOI8-R \
		KOI8-U \
		SJIS \
		US-ASCII \
		UTF-8

# CLDR files
CLDRFILES_CORE= https://unicode.org/Public/cldr/35/core.zip
CLDRFILES_KEY=	https://unicode.org/Public/cldr/35/keyboards.zip
CLDRFILES_TOOLS=https://unicode.org/Public/cldr/35/tools.zip
CLDRFILES_UCD=	http://www.unicode.org/Public/zipped/latest/UCD.zip

# fetch and extract targets
${UNIDIR}:
	mkdir -p ${UNIDIR}
.for N in CORE KEY TOOLS UCD
${CLDRFILES_${N}:T}:
	fetch ${CLDRFILES_${N}}
fetch: ${CLDRFILES_${N}:T}
extract-${CLDRFILES_${N}:T}:: ${CLDRFILES_${N}:T} ${UNIDIR}
	cd ${UNIDIR} && unzip -o ../${CLDRFILES_${N}:T}
extract: extract-${CLDRFILES_${N}:T}
.endfor
	grep 'name="version"' ${UNIDIR}/tools/build.xml | \
		sed 's/.* value="//;s/".*//' > ${UNIDIR}/cldr-version
patch::
.if exists(${PATCHDIR})
	cd ${UNIDIR} && cat ${PATCHDIR}/patch-* | patch
.endif

.if !exists(${UNIDIR}/tools/java/cldr.jar)
.ORDER: extract patch
build-tools: extract patch tools-test ${UNIDIR}
	cd ${UNIDIR}/tools/java && ${SETENV} ant all jar
.else
build-tools:
	@echo cldr.jar is ready.
.endif

JAVA_CLDR= java -DCLDR_DIR=${UNIDIR:Q} -jar ${UNIDIR}/tools/java/cldr.jar

posix: posixcm post-posixcm posixsrc posixcol
.ORDER: posixcm post-posixcm posixsrc posixcol
${UNIDIR}/posix:
	ln -s -f ../posix ${.TARGET}
clean-posix:
	rm -rf posix ${UNIDIR}/posix
post-posixcm: ${UNIDIR}/posix
	perl -I ${TOOLSDIR} ${TOOLSDIR}/utf8-rollup.pl \
	    --unidir=${UNIDIR}
.for enc in ${ENCODINGS}
posixcm: build-tools posix/${enc}.cm
.ORDER: build-tools posix/${enc}.cm
posix/${enc}.cm:
	mkdir -p posix && \
	    ${JAVA_CLDR} org.unicode.cldr.posix.GenerateCharmap \
		-d posix -c ${enc}
.endfor
.for area in ${BASE_LOCALES_OF_INTEREST}
posixsrc: build-tools posix/${area}.UTF-8.src
.ORDER: build-tools posix/${area}.UTF-8.src
posix/${area}.UTF-8.src:
	mkdir -p posix && \
	    ${JAVA_CLDR} org.unicode.cldr.posix.GeneratePOSIX \
		-d posix -m ${area} -c UTF-8
.endfor
.for area encoding in ${COLLATION_SPECIAL}
posixcol: build-tools posix/${area}.${encoding}.src
.ORDER: build-tools posix/${area}.${encoding}.src
posix/${area}.${encoding}.src:
	mkdir -p posix && \
	    ${JAVA_CLDR} org.unicode.cldr.posix.GeneratePOSIX \
		-d posix -m ${area} -c ${encoding}
.endfor

# generate widths.txt using the data from libut8proc
GETWIDTHS=${TOOLSDIR}/getwidths
MKWIDTHS=${TOOLSDIR}/mkwidths.pl
WIDTHS=	${ETCDIR}/final-maps/widths.txt

U8CFLAGS!=pkgconf --cflags libutf8proc
U8LIBS!=pkgconf --libs libutf8proc
CFLAGS+=${U8CFLAGS}
LDFLAGS+=${U8LIBS}

CLEANFILES+=${TOOLSDIR}/getwidths

widths:	${WIDTHS}
${WIDTHS}: posixcm ${GETWIDTHS}
	${GETWIDTHS} | ${MKWIDTHS} ${.OBJDIR}/posix/UTF-8.cm ${.TARGET}

.include <bsd.obj.mk>
