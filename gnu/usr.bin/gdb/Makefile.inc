#	$Id$

CFLAGS+=-I${GDBDIR}/include -I${GDBDIR}/gdb -I${GDBDIR}/bfd
CFLAGS+=-I${GDBDIR}/libiberty -I${GDBDIR}/gdb/config
CFLAGS+=-DHAVE_CONFIG_H

.include "../Makefile.inc"
