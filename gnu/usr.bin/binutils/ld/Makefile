#
# $Id: Makefile,v 1.2 1998/03/08 08:08:39 peter Exp $
#

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/ld

PROG=		ld
SCRIPTDIR=	/usr/libdata/ldscripts
SRCS+=		ldcref.c ldctor.c ldemul.c ldexp.c ldfile.c \
		ldgram.c ldlang.c ldlex.l ldmain.c ldmisc.c \
		ldver.c ldwrite.c lexsup.c mri.c
CFLAGS+=	-DDEFAULT_EMULATION=\"${EMULATION}\"
CFLAGS+=	-DSCRIPTDIR=\"${DESTDIR}/usr/libdata\"
CFLAGS+=	-DTARGET=\"${TARGET}\"
CFLAGS+=	-I${SRCDIR}/ld
LDADD+=		-L${RELTOP}/libbfd -lbfd
LDADD+=		-L${RELTOP}/libiberty -liberty
CLEANFILES+=	ldgram.c ldgram.h ldlex.c
CLEANDIRS+=	ldscripts

beforedepend: ldgram.c ldgram.h ldlex.c

ldgram.c ldgram.h: ldgram.y
	${YACC} ${YFLAGS} -o ${.TARGET:S/.h$/.c/} ${.ALLSRC}

afterinstall:
	${INSTALL} ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${LDSCRIPTS:S|^|ldscripts/|} ${DESTDIR}${SCRIPTDIR}

.include <bsd.prog.mk>
