INFO = gdb gdbint stabs annotate

.PATH: ${.CURDIR}/../../../../contrib/gdb/gdb/doc

MAKEINFOFLAGS += -I ${.CURDIR}/../../../../contrib/gdb/gdb/doc
MAKEINFOFLAGS += -I ${.CURDIR}/../../../../contrib/gdb/readline/doc

INFOSECTION= "Gdb Documentation"
INFOENTRY_annotate= "* GDB annotation: (annotate).	Annotations for the GNU Debugger (GDB)."

MAKEINFOFLAGS += -I ${.CURDIR}/../../../lib/libreadline/doc

.include <bsd.info.mk>
