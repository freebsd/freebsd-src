#	$Id: Makefile.inc,v 1.6 1998/10/15 14:15:09 bde Exp $

CFLAGS+=-I${GDBDIR}/include -I${GDBDIR}/gdb -I${GDBDIR}/bfd
CFLAGS+=-I${GDBDIR}/libiberty -I${GDBDIR}/gdb/config
CFLAGS+=-DHAVE_CONFIG_H

.include "../Makefile.inc"
