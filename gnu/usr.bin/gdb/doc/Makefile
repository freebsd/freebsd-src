#	$Id$

INFO=	annotate gdb gdbint stabs

CONTRIBDIR= ${.CURDIR}/../../../../contrib
SRCDIR=	${CONTRIBDIR}/gdb/gdb/doc
MAKEINFOFLAGS+= -I ${CONTRIBDIR}/libreadline/doc
CLEANFILES= gdb-cfg.texi inc-hist.texi inc-hist.texi.orig

INFOSECTION= "Gdb Documentation"
INFOENTRY_annotate= "* GDB annotation: (annotate).	Annotations for the GNU Debugger (GDB)."

gdb.info: gdb.texinfo gdb-cfg.texi GDBvn.texi remote.texi \
	rluser.texinfo inc-hist.texi

gdb-cfg.texi: all-cfg.texi
	ln -sf ${.ALLSRC} ${.TARGET}

.PATH: ${CONTRIBDIR}/libreadline/doc
inc-hist.texi: hsuser.texinfo inc-hist.diff
	cp ${.ALLSRC:M*.texinfo} ${.TARGET}
	patch -b .orig < ${.ALLSRC:M*.diff}

.include <bsd.info.mk>
