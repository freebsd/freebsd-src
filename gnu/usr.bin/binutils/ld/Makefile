#
# $Id: Makefile,v 1.1 1998/03/12 02:55:20 jdp Exp $
#

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/ld

PROG=		ld
SCRIPTDIR=	/usr/libdata/ldscripts
SRCS+=		ldcref.c ldctor.c ldemul.c ldexp.c ldfile.c \
		ldgram.y ldlang.c ldlex.l ldmain.c ldmisc.c \
		ldver.c ldwrite.c lexsup.c mri.c
CFLAGS+=	-DDEFAULT_EMULATION=\"${EMULATION}\"
CFLAGS+=	-DSCRIPTDIR=\"${DESTDIR}/usr/libdata\"
CFLAGS+=	-DTARGET=\"${TARGET}\"
CFLAGS+=	-I${SRCDIR}/ld
LDADD+=		-L${RELTOP}/libbfd -lbfd
LDADD+=		-L${RELTOP}/libiberty -liberty
CLEANDIRS+=	ldscripts

afterinstall:
	${INSTALL} ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${LDSCRIPTS:S|^|ldscripts/|} ${DESTDIR}${SCRIPTDIR}

.include <bsd.prog.mk>
