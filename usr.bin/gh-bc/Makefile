# $FreeBSD$

.include <src.opts.mk>

PROG=		gh-bc
PROGNAME=	bc

BCDIR=		${SRCTOP}/contrib/${PROGNAME}
BCVERSION!=	sed -n -e '/.*VERSION *= *[0-9]/s/.*VERSION *= *//p' ${BCDIR}/Makefile.in

SRCS=		args.c data.c file.c lang.c lex.c main.c num.c parse.c program.c read.c vector.c vm.c
SRCS+=		bc/bc.c bc/lex.c bc/parse.c dc/dc.c dc/lex.c dc/parse.c history/history.c
SRCS+=		bc_help.c dc_help.c lib.c lib2.c opt.c rand/rand.c
MAN=		bc.1 dc.1

LINKS=		${BINDIR}/bc ${BINDIR}/dc

.PATH:		${BCDIR}/src ${BCDIR}/gen ${BCDIR}/manuals ${.OBJDIR}

CATALOGS=	en_US.UTF-8
CATALOGS+=	de_DE.UTF-8 de_DE.ISO8859-1
CATALOGS+=	es_ES.UTF-8 es_ES.ISO8859-1
CATALOGS+=	fr_FR.UTF-8 fr_FR.ISO8859-1
CATALOGS+=	ja_JP.UTF-8 ja_JP.eucJP
CATALOGS+=	nl_NL.UTF-8 nl_NL.ISO8859-1
CATALOGS+=	pl_PL.UTF-8 pl_PL.ISO8859-2
CATALOGS+=	pt_PT.UTF-8 pt_PT.ISO8859-1
CATALOGS+=	ru_RU.UTF-8 ru_RU.ISO8859-5 ru_RU.CP1251 ru_RU.CP866 ru_RU.KOI8-R
CATALOGS+=	zh_CN.UTF-8 zh_CN.eucCN zh_CN.GB18030 zh_CN.GB2312 zh_CN.GBK

NLSNAME=	bc
NLSSRCDIR=	${BCDIR}/locales

CFLAGS+=	-DMAINEXEC=${PROGNAME}
CFLAGS+=	-DNLSPATH=/usr/share/nls/%L/%N.cat
CFLAGS+=	-DBC_ENABLED
CFLAGS+=	-DBC_ENABLE_PROMPT
CFLAGS+=	-DBC_ENABLE_LONG_OPTIONS
CFLAGS+=	-DBC_ENABLE_EXTRA_MATH
CFLAGS+=	-DBC_ENABLE_HISTORY
CFLAGS+=	-DBC_ENABLE_RAND
CFLAGS+=	-DDC_ENABLED
CFLAGS+=	-DNDEBUG
CFLAGS+=	-DVERSION=${BCVERSION}
CFLAGS+=	-I${BCDIR}/include

.if ${MK_NLS_CATALOGS} == "no"
CFLAGS+=	-DBC_ENABLE_NLS=0
MAN_SRC_BC=	bc/N.1
MAN_SRC_DC=	dc/N.1
.else
CFLAGS+=	-DBC_ENABLE_NLS=1
MAN_SRC_BC=	bc/A.1
MAN_SRC_DC=	dc/A.1

# prevent floating point incompatibilities caused by -flto on some architectures
.if ${MACHINE_ARCH} != mips && ${MACHINE_ARCH} != mips64 && \
    ${MACHINE_ARCH} != powerpc64 && ${MACHINE_ARCH} != riscv64
CFLAGS+=	-flto
.endif

.for catalog in ${CATALOGS}
NLS+=           ${catalog:C/.*://}
NLSSRCFILES_${catalog:C/.*://}= ${catalog:C/.*://}.msg
.endfor

NLSLINKS_en_US.UTF-8+=	en_AU.UTF-8 en_CA.UTF-8 en_GB.UTF-8 en_IE.UTF-8 \
			en_NZ.UTF-8 C
NLSLINKS_en_US.UTF-8+=	en_AU.US-ASCII en_CA.US-ASCII en_GB.US-ASCII \
			en_NZ.US-ASCII
NLSLINKS_en_US.UTF-8+=	en_AU.ISO8859-1 en_CA.ISO8859-1 en_GB.ISO8859-1 \
			en_NZ.ISO8859-1 en_US.ISO8859-1
NLSLINKS_en_US.UTF-8+=	en_AU.ISO8859-15 en_CA.ISO8859-15 en_GB.ISO8859-15 \
			en_NZ.ISO8859-15 en_US.ISO8859-15

NLSLINKS_de_DE.UTF-8+=	de_AT.UTF-8 de_CH.UTF-8
NLSLINKS_de_DE.ISO8859-1+= de_AT.ISO8859-1 de_CH.ISO8859-1
NLSLINKS_de_DE.ISO8859-1+= de_AT.ISO8859-15 de_CH.ISO8859-15 de_DE.ISO8859-15

NLSLINKS_es_ES.ISO8859-1+= es_ES.ISO8859-15

NLSLINKS_fr_FR.UTF-8+=	fr_BE.UTF-8 fr_CA.UTF-8 fr_CH.UTF-8
NLSLINKS_fr_FR.ISO8859-1+= fr_BE.ISO8859-1 fr_CA.ISO8859-1 fr_CH.ISO8859-1
NLSLINKS_fr_FR.ISO8859-1+= fr_BE.ISO8859-15 fr_CA.ISO8859-15 fr_CH.ISO8859-15 \
			fr_FR.ISO8859-15

NLSLINKS_nl_NL.ISO8859-1+= nl_BE.ISO8859-1
NLSLINKS_nl_NL.ISO8859-1+= nl_BE.ISO8859-15 nl_NL.ISO8859-15

NLSLINKS_pt_PT.UTF-8+=	pt_BR.UTF-8
NLSLINKS_pt_PT.ISO8859-1+= pt_BR.ISO8859-1
NLSLINKS_pt_PT.ISO8859-1+= pt_PT.ISO8859-15
.endif

lib.c:		lib.bc
		cd ${BCDIR} && sh gen/strgen.sh gen/lib.bc ${.OBJDIR}/lib.c bc_lib bc.h bc_lib_name 1 1

lib2.c:		lib2.bc
		cd ${BCDIR} && sh gen/strgen.sh gen/lib2.bc ${.OBJDIR}/lib2.c bc_lib2 bc.h bc_lib2_name 1 1

bc_help.c:	bc_help.txt
		cd ${BCDIR} && sh gen/strgen.sh gen/bc_help.txt ${.OBJDIR}/bc_help.c bc_help bc.h

dc_help.c:	dc_help.txt
		cd ${BCDIR} && sh gen/strgen.sh gen/dc_help.txt ${.OBJDIR}/dc_help.c dc_help dc.h

bc.1:
		${CP} ${BCDIR}/manuals/${MAN_SRC_BC} ${.OBJDIR}/bc.1

dc.1:
		${CP} ${BCDIR}/manuals/${MAN_SRC_DC} ${.OBJDIR}/dc.1

.include <bsd.prog.mk>
