#	$Id: Makefile.inc,v 1.4 1998/05/01 14:37:36 bde Exp $

CFLAGS+=-I${GDBDIR}/include -I${GDBDIR}/gdb -I${GDBDIR}/bfd
CFLAGS+=-I${GDBDIR}/libiberty -I${GDBDIR}/gdb/config
CFLAGS+=-DHAVE_CONFIG_H

.if ${OBJFORMAT} == elf
BINDIR?=	/usr/libexec/elf
.else
BINDIR?=	/usr/libexec/aout
.endif

.include "../Makefile.inc"
