# $FreeBSD$

ELF_SCR_EXT=	x xbn xc xd xdc xdw xn xr xs xsc xsw xu xw
.include "../Makefile.inc0"
.include <src.opts.mk>

.PATH: ${SRCDIR}/ld

PROG=	ld.bfd
MAN=	ld.1
SCRIPTDIR= /usr/libdata/ldscripts
SRCS+=	ldcref.c \
	ldctor.c \
	ldemul-list.h \
	ldemul.c \
	ldexp.c \
	ldfile.c \
	ldgram.y \
	ldlang.c \
	ldlex.l \
	ldmain.c \
	ldmisc.c \
	ldver.c \
	ldwrite.c \
	lexsup.c \
	mri.c

CFLAGS+= -DTARGET=\"${TARGET_TUPLE}\"
CFLAGS+= -DDEFAULT_EMULATION=\"${NATIVE_EMULATION}\"
CFLAGS+= -DSCRIPTDIR=\"${TOOLS_PREFIX}/usr/libdata\"
CFLAGS+= -DBFD_VERSION_STRING=\"${VERSION}\"
CFLAGS+= -DBINDIR=\"${BINDIR}\"
.if defined(TOOLS_PREFIX)
CFLAGS+= -DTARGET_SYSTEM_ROOT=\"${TOOLS_PREFIX}\"
.else
CFLAGS+= -DTARGET_SYSTEM_ROOT=\"/\"
.endif
CFLAGS+= -DTOOLBINDIR=\"${TOOLS_PREFIX}/${BINDIR}/libexec\"
CFLAGS+= -D_GNU_SOURCE
CFLAGS+= -I${SRCDIR}/ld -I${SRCDIR}/bfd
.if ${MK_SHARED_TOOLCHAIN} == "no"
NO_SHARED?= yes
.endif
DPADD=	${RELTOP}/libbfd/libbfd.a
DPADD+=	${RELTOP}/libiberty/libiberty.a
LDADD=	${DPADD}
CLEANDIRS+=	ldscripts
CLEANFILES+=	ldemul-list.h stringify.sed

FILES=		${LDSCRIPTS:S|^|ldscripts/|}
FILESDIR=	${SCRIPTDIR}
.if ${MK_LLD_AS_LD} == "no"
LINKS=		${BINDIR}/ld.bfd ${BINDIR}/ld
.endif

HOST=		${TARGET_TUPLE}
LIBSEARCHPATH=	\"=/lib\":\"=/usr/lib\"
.for ext in ${ELF_SCR_EXT}
LDSCRIPTS+=	${NATIVE_EMULATION}.${ext}
ldscripts/${NATIVE_EMULATION}.${ext}: e${NATIVE_EMULATION}.c
.endfor

EMXFR=
EMLST=
.for _e in ${NATIVE_EMULATION} ${EMS}
EMXFR+=	extern ld_emulation_xfer_type ld_${_e}_emulation;
EMLST+=	&ld_${_e}_emulation,
.endfor

ldemul-list.h:
	echo "${EMXFR}" > ${.TARGET}
	echo "#define EMULATION_LIST ${EMLST} 0" >> ${.TARGET}

stringify.sed: ${SRCDIR}/ld/emultempl/astring.sed .NOMETA
	ln -sf ${.ALLSRC} ${.TARGET}

GENDIRDEPS_FILTER.host+= Nusr.bin/yacc

.include <bsd.prog.mk>
