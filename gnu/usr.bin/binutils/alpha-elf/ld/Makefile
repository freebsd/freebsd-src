#
# $Id: Makefile,v 1.1.1.1 1998/03/01 23:41:02 jdp Exp $
#

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/ld

EMULATION=	elf64alpha
LDSCRIPTS=	elf64alpha.x elf64alpha.xbn elf64alpha.xn elf64alpha.xr \
		elf64alpha.xs elf64alpha.xu alpha.x alpha.xbn \
		alpha.xn alpha.xr alpha.xu

PROG=		ld
BINDIR=		/usr/libexec/elf
SCRIPTDIR=	/usr/libdata/ldscripts
SRCS=		ldgram.c eelf64alpha.c ealpha.c ldcref.c \
		ldctor.c  ldemul.c ldexp.c ldfile.c ldlang.c \
		ldlex.l ldmain.c ldmisc.c ldver.c ldwrite.c \
		lexsup.c mri.c
CFLAGS+=	-DDEFAULT_EMULATION=\"${EMULATION}\"
CFLAGS+=	-DSCRIPTDIR=\"${DESTDIR}/usr/libdata\"
CFLAGS+=	-DTARGET=\"${TARGET}\"
CFLAGS+=	-I${SRCDIR}/ld
LDADD+=		-L${RELTOP}/libbfd -lbfd
LDADD+=		-L${RELTOP}/libiberty -liberty
CLEANFILES+=	eelf64alpha.c ealpha.c ldgram.c ldgram.h ldlex.c
CLEANDIRS+=	ldscripts

beforedepend: eelf64alpha.c ealpha.c ldgram.c ldgram.h ldlex.c

eelf64alpha.c:	emulparams/elf64alpha.sh emultempl/elf32.em \
		scripttempl/elf.sc genscripts.sh emultempl/stringify.sed
	sh ${SRCDIR}/ld/genscripts.sh ${SRCDIR}/ld ${DESTDIR}/usr/lib \
	    ${HOST} ${TARGET} ${TARGET} ${EMULATION} "" \
	    ${EMULATION} ${TARGET}

ealpha.c:	emulparams/elf64alpha.sh emultempl/elf32.em \
		scripttempl/elf.sc genscripts.sh emultempl/stringify.sed
	sh ${SRCDIR}/ld/genscripts.sh ${SRCDIR}/ld ${DESTDIR}/usr/lib \
	    ${HOST} ${TARGET} ${TARGET} ${EMULATION} "" \
	    alpha alpha-unknown-freebsd

ldgram.c ldgram.h: ldgram.y
	${YACC} ${YFLAGS} -o ${.TARGET:S/.h$/.c/} ${.ALLSRC}

afterinstall:
	${INSTALL} ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${LDSCRIPTS:S|^|ldscripts/|} ${DESTDIR}${SCRIPTDIR}

.include <bsd.prog.mk>
