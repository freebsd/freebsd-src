INFO = gdb gdbint stabs annotate

GDBDIR=	${.CURDIR}/../../../../contrib/gdb
.PATH: ${GDBDIR}/gdb/doc

INFOSECTION= "Gdb Documentation"
INFOENTRY_annotate= "* GDB annotation: (annotate).	Annotations for the GNU Debugger (GDB)."

MAKEINFOFLAGS += -I ${GDBDIR}/gdb/doc
MAKEINFOFLAGS += -I ${GDBDIR}/../libreadline/doc
MAKEINFOFLAGS += -I ${GDBDIR}/readline/doc

.include <bsd.info.mk>
