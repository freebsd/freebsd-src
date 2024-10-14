# See https://wiki.freebsd.org/LocaleNewApproach
# Taken from FreeBSD svn [base]/user/edwin/locale/cldr
#
# needs:
# devel/p5-Tie-IxHash
#
# Modified by John Marino to suit DragonFly needs
#

CLDRVERSION=	45.0
UCDVERSION=	15.0.0

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

PKGS=	openjdk11 p5-XML-Parser p5-Tie-IxHash p5-Text-Iconv utf8proc
tools-test:
	@pkg info -e ${PKGS} || ( echo required packages: ${PKGS}; exit 1 )

KNOWN=		monetdef numericdef msgdef colldef ctypedef # timedef
TYPES?=		${KNOWN}

SETENV=	env -i \
	PATH="${PATH}" \
	TMPDIR="${TMPDIR}" \
	UNIDIR="${UNIDIR}" \
	BASEDIR="${BASEDIR}" \
	TOOLSDIR="${TOOLSDIR}" \
	ETCDIR="${ETCDIR}"

all: tools-test posix build afterbuild widths
.ORDER: tools-test posix build afterbuild widths

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
.for t in ${TYPES:Nctypedef}
.  if ${KNOWN:M${t}}
install: install-${t}
install-${t}:
	cd ${LOCALESRCDIR}/${t}_unicode && \
	    rm -f Makefile *.src && \
	    cd ${.OBJDIR} && \
	    install -m 644 ${t}/* ${LOCALESRCDIR}/${t}_unicode
.  endif
.endfor
install: install-ctypedef
install-ctypedef:
	cd ${LOCALESRCDIR}/ctypedef && \
		rm -f C.UTF-8.src && \
		cd ${.OBJDIR} && \
		install -m 644 ctypedef/C.UTF-8.src ${LOCALESRCDIR}/ctypedef

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

BASE_LOCALES_OF_INTEREST?= \
	af_ZA am_ET ar_AE ar_EG ar_JO ar_MA ar_QA ar_SA \
	be_BY bg_BG ca_AD ca_ES ca_FR ca_IT \
	cs_CZ da_DK de_AT de_CH de_DE el_GR en_AU en_CA \
	en_GB en_HK en_IE en_NZ en_PH en_SG en_US en_ZA \
	es_AR es_CR es_ES es_MX et_EE eu_ES fa_AF fa_IR fi_FI fr_BE \
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

ENCODINGS=	UTF-8 \
		UTF-32

# CLDR files
CLDRFILES_CORE= https://unicode.org/Public/cldr/${CLDRVERSION:R}/core.zip
CLDRFILES_KEY=	https://unicode.org/Public/cldr/${CLDRVERSION:R}/cldr-keyboards-${CLDRVERSION}.zip
CLDRFILES_TOOLS=https://unicode.org/Public/cldr/${CLDRVERSION:R}/cldr-tools-${CLDRVERSION}.jar
CLDRFILES_UCD=	https://www.unicode.org/Public/zipped/${UCDVERSION}/UCD.zip

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
	echo ${CLDRVERSION} > ${UNIDIR}/cldr-version
	mkdir -p ${UNIDIR}/seed/main
patch::
.if exists(${PATCHDIR})
	cd ${UNIDIR} && cat ${PATCHDIR}/patch-* | patch
.endif

JAVA_CLDR= java -DCLDR_DIR=${UNIDIR:Q} -jar ${.OBJDIR:tA}/${CLDRFILES_TOOLS:T}

posix: posixcm post-posixcm posixsrc
.ORDER: posixcm post-posixcm posixsrc
${UNIDIR}/posix: ${UNIDIR}
	ln -s -f ../posix ${.TARGET}
clean-posix:
	rm -rf posix ${UNIDIR}/posix
${UNIDIR}/posix/xx_Comm_C.UTF-8.src: ${UNIDIR}/posix
	perl -I ${TOOLSDIR} ${TOOLSDIR}/utf8-rollup.pl \
	    --unidir=${UNIDIR}
post-posixcm: ${UNIDIR}/posix/xx_Comm_C.UTF-8.src
.for enc in ${ENCODINGS}
posixcm: posix/${enc}.cm
.ORDER: posix/${enc}.cm
posix/${enc}.cm:
	mkdir -p posix && \
	    ${JAVA_CLDR} org.unicode.cldr.posix.GenerateCharmap \
		-d posix -c ${enc}
.endfor
.for area in ${BASE_LOCALES_OF_INTEREST}
posixsrc: posix/${area}.UTF-8.src
.ORDER: posix/${area}.UTF-8.src
posix/${area}.UTF-8.src:
	mkdir -p posix && \
	    ${JAVA_CLDR} org.unicode.cldr.posix.GeneratePOSIX \
		-d posix -m ${area} -c UTF-8
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
