#	$Id: Makefile.inc,v 1.5 1998/10/15 00:15:01 peter Exp $

CFLAGS+=-I${GDBDIR}/include -I${GDBDIR}/gdb -I${GDBDIR}/bfd
CFLAGS+=-I${GDBDIR}/libiberty -I${GDBDIR}/gdb/config
CFLAGS+=-DHAVE_CONFIG_H

.include "../Makefile.inc"
