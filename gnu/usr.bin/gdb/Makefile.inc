# bsd.lib.mk causes this file to be loaded multiple times.
.if !defined(BEEN_HERE)
BEEN_HERE=1	

# this may be defined in other places
.if !defined(GDBDIR)
GDBDIR=	${.CURDIR}/../../../../contrib/gdb
.endif

CFLAGS+= -I${GDBDIR}/include/. -I${GDBDIR}/gdb/. -I${GDBDIR}/bfd/.
CFLAGS+= -I${GDBDIR}/libiberty/.
CFLAGS+= -I${GDBDIR}/gdb/config/.
CFLAGS+= -DHAVE_CONFIG_H
.endif
