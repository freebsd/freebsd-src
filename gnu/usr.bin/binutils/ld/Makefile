#
# $FreeBSD$
#

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/ld

PROG=		ld
SCRIPTDIR=	/usr/libdata/ldscripts
SRCS+=		ldcref.c ldctor.c ldemul.c ldemul-list.h ldexp.c ldfile.c \
		ldgram.y ldlang.c ldlex.l ldmain.c ldmisc.c \
		ldver.c ldwrite.c lexsup.c mri.c
CFLAGS+=	-DSCRIPTDIR=\"${DESTDIR}/usr/libdata\"
CFLAGS+=	-I${SRCDIR}/ld
LDADD+=		-L${RELTOP}/libbfd -lbfd
LDADD+=		-L${RELTOP}/libiberty -liberty
DPADD+=		${RELTOP}/libbfd/libbfd.a
DPADD+=		${RELTOP}/libiberty/libiberty.a
CLEANDIRS+=	ldscripts
CLEANFILES+=	ldemul-list.h

# If set, BINUTILSDISTDIR is the path to a directory containing the full GNU
# binutils release. FreeBSD only distributes the bits that are required to
# build native architectures. BINUTILSDISTDIR is needed to build cross tools.
.if	defined(BINUTILSDISTDIR) && exists(${BINUTILSDISTDIR})
.PATH:		${BINUTILSDISTDIR}/ld
CFLAGS+=	-I${BINUTILSDISTDIR}/ld
.endif

EMXFR=
EMLST=
.for _e in ${EMS}
EMXFR+= extern ld_emulation_xfer_type ${_e};
EMLST+=	&${_e},
.endfor

ldemul-list.h:
	echo "${EMXFR}" > ldemul-list.h
	echo "#define EMULATION_LIST ${EMLST} 0" >> ldemul-list.h

afterinstall:
	${INSTALL} ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${LDSCRIPTS:S|^|ldscripts/|} ${DESTDIR}${SCRIPTDIR}

.include <bsd.prog.mk>
