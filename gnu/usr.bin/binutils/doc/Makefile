# $Id: Makefile,v 1.1 1998/10/03 03:59:47 jdp Exp $

.include "../Makefile.inc0"

GDBDIR=	${.CURDIR}/../../../../contrib/gdb
CONTRIBDIR= ${.CURDIR}/../../../../contrib

.PATH: ${SRCDIR}/gas/doc ${SRCDIR}/ld ${SRCDIR}/bfd/doc ${GDBDIR}/gdb/doc

INFO = as ld annotate gdb gdbint stabs
INFOSECTION=	"Programming & development tools."
INFOENTRY_as=	"* As: (as).            The GNU assembler."
INFOENTRY_ld=	"* Ld: (ld).		The GNU linker."
INFOENTRY_annotate= "* GDB annotation: (annotate).	Annotations for the GNU Debugger (GDB)."

MAKEINFOFLAGS+= --no-validate
MAKEINFOFLAGS+= -I ${SRCDIR}/gas/doc -I ${SRCDIR}/ld -I ${SRCDIR}/bfd/doc
MAKEINFOFLAGS+= -I ${GDBDIR}/gdb/doc
MAKEINFOFLAGS+= -I ${CONTRIBDIR}/libreadline/doc

CLEANFILES= gdb-cfg.texi inc-hist.texi inc-hist.texi.orig

as.info:	as.texinfo asconfig.texi c-i386.texi
ld.info:	ld.texinfo bfdsumm.texi

gdb.info: gdb.texinfo gdb-cfg.texi GDBvn.texi remote.texi \
	rluser.texinfo inc-hist.texi

gdb-cfg.texi: all-cfg.texi
	ln -sf ${.ALLSRC} ${.TARGET}

.PATH: ${CONTRIBDIR}/libreadline/doc
inc-hist.texi: hsuser.texinfo inc-hist.diff
	cp ${.ALLSRC:M*.texinfo} ${.TARGET}
	patch -b .orig < ${.ALLSRC:M*.diff}

.include <bsd.info.mk>
