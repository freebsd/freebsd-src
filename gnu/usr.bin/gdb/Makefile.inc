
# this may be defined in other places
.if !defined(GDBDIR)
GDBDIR=	${.CURDIR}/../../../../contrib/gdb
.endif

CFLAGS+= -I${GDBDIR}/include/. -I${GDBDIR}/gdb/. -I${GDBDIR}/bfd/.
CFLAGS+= -I${GDBDIR}/libiberty/.
CFLAGS+= -I${GDBDIR}/gdb/config/.
CFLAGS+= -DHAVE_CONFIG_H

