.if !defined(GDB_MAKEFILE_INC_BEEN_HERE)
GDB_MAKEFILE_INC_BEEN_HERE=yes

# this may be defined in other places
.if !defined(GDBDIR)
GDBDIR=	${.CURDIR}/../../../../contrib/gdb
.endif

CFLAGS+= -I${GDBDIR}/include/. -I${GDBDIR}/gdb/. -I${GDBDIR}/bfd/.
CFLAGS+= -I${GDBDIR}/libiberty/.
CFLAGS+= -I${GDBDIR}/gdb/config/.
CFLAGS+= -DHAVE_CONFIG_H

.endif
