# $FreeBSD$

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/ld

PROG=	ld
SCRIPTDIR= /usr/libdata/ldscripts
SRCS+=	ldcref.c ldctor.c ldemul.c ldemul-list.h ldexp.c ldfile.c \
	ldgram.y ldlang.c ldlex.l ldmain.c ldmisc.c \
	ldver.c ldwrite.c lexsup.c mri.c

.if defined(BOOTSTRAPPING) && !exists(/usr/include/elf-hints.h)
SRCS+=	elf-hints.h
CLEANFILES+=	elf-hints.h

elf-hints.h:
	ln -sf ${.CURDIR}/../../../../include/${.TARGET} .
.endif

CFLAGS+= -DSCRIPTDIR=\"${DESTDIR}/usr/libdata\"
CFLAGS+= -DBFD_VERSION_STRING=\"${VERSION}\"
CFLAGS+= -I${SRCDIR}/ld -I${SRCDIR}/bfd
NOSHARED?= yes
DPADD=	${RELTOP}/libbfd/libbfd.a
DPADD+=	${RELTOP}/libiberty/libiberty.a
LDADD=	${DPADD}
CLEANDIRS+=	ldscripts
CLEANFILES+=	ldemul-list.h stringify.sed

# If set, BINUTILSDISTDIR is the path to a directory containing the full GNU
# binutils release. FreeBSD only distributes the bits that are required to
# build native architectures. BINUTILSDISTDIR is needed to build cross tools.
.if defined(BINUTILSDISTDIR) && exists(${BINUTILSDISTDIR})
.PATH: ${BINUTILSDISTDIR}/ld
CFLAGS+= -I${BINUTILSDISTDIR}/ld
.endif

EMXFR=
EMLST=
.for _e in ${EMS}
EMXFR+=	extern ld_emulation_xfer_type ${_e};
EMLST+=	&${_e},
.endfor

ldemul-list.h:
	echo "${EMXFR}" > ${.TARGET}
	echo "#define EMULATION_LIST ${EMLST} 0" >> ${.TARGET}

stringify.sed:
	ln -sf ${SRCDIR}/ld/emultempl/astring.sed ${.TARGET}

afterinstall:
	${INSTALL} ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${LDSCRIPTS:S|^|ldscripts/|} ${DESTDIR}${SCRIPTDIR}

.include <bsd.prog.mk>
