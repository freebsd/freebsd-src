#
# $Id$
#

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/ld

EMULATION=	elf_i386
LDSCRIPTS=	elf_i386.x elf_i386.xbn elf_i386.xn elf_i386.xr \
		elf_i386.xs elf_i386.xu i386freebsd.x i386freebsd.xbn \
		i386freebsd.xn i386freebsd.xr i386freebsd.xu

PROG=		ld
BINDIR=		/usr/libexec/elf
SCRIPTDIR=	/usr/libdata/ldscripts
SRCS=		ldgram.c eelf_i386.c ei386freebsd.c ldcref.c \
		ldctor.c  ldemul.c ldexp.c ldfile.c ldlang.c \
		ldlex.l ldmain.c ldmisc.c ldver.c ldwrite.c \
		lexsup.c mri.c
CFLAGS+=	-DDEFAULT_EMULATION=\"${EMULATION}\"
CFLAGS+=	-DSCRIPTDIR=\"${DESTDIR}/usr/libdata\"
CFLAGS+=	-DTARGET=\"${TARGET}\"
CFLAGS+=	-I${SRCDIR}/ld
LDADD+=		-L${RELTOP}/libbfd -lbfd
LDADD+=		-L${RELTOP}/libiberty -liberty
CLEANFILES+=	eelf_i386.c ei386freebsd.c ldgram.c ldgram.h ldlex.c
CLEANDIRS+=	ldscripts

beforedepend: eelf_i386.c ei386freebsd.c ldgram.c ldgram.h ldlex.c

eelf_i386.c:	emulparams/elf_i386.sh emultempl/elf32.em \
		scripttempl/elf.sc genscripts.sh emultempl/stringify.sed
	sh ${SRCDIR}/ld/genscripts.sh ${SRCDIR}/ld ${DESTDIR}/usr/lib \
	    ${HOST} ${TARGET} ${TARGET} ${EMULATION} "" \
	    ${EMULATION} ${TARGET}

ei386freebsd.c:	emulparams/elf_i386.sh emultempl/elf32.em \
		scripttempl/elf.sc genscripts.sh emultempl/stringify.sed
	sh ${SRCDIR}/ld/genscripts.sh ${SRCDIR}/ld ${DESTDIR}/usr/lib \
	    ${HOST} ${TARGET} ${TARGET} ${EMULATION} "" \
	    i386freebsd i386-unknown-freebsd

ldgram.c ldgram.h: ldgram.y
	${YACC} ${YFLAGS} -o ${.TARGET:S/.h$/.c/} ${.ALLSRC}

afterinstall:
	${INSTALL} ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${LDSCRIPTS:S|^|ldscripts/|} ${DESTDIR}${SCRIPTDIR}

.include <bsd.prog.mk>
