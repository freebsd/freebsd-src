#
# $Id: Makefile,v 1.2 1998/05/04 16:31:41 bde Exp $
#

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/ld

PROG=		ld
SCRIPTDIR=	/usr/libdata/ldscripts
SRCS+=		ldcref.c ldctor.c ldemul.c ldexp.c ldfile.c \
		ldgram.y ldlang.c ldlex.l ldmain.c ldmisc.c \
		ldver.c ldwrite.c lexsup.c mri.c
CFLAGS+=	-DSCRIPTDIR=\"${DESTDIR}/usr/libdata\"
CFLAGS+=	-I${SRCDIR}/ld
LDADD+=		-L${RELTOP}/libbfd -lbfd
LDADD+=		-L${RELTOP}/libiberty -liberty
CLEANDIRS+=	ldscripts

beforedepend: ldemul-list.h

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
