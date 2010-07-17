# $FreeBSD: src/gnu/usr.bin/gdb/arch/mips/Makefile,v 1.1.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.if !defined(GDB_CROSS_DEBUGGER)
LIBSRCS+= mips-nat.c mips-nat.c mipsfbsd-nat.c
.endif
LIBSRCS+= solib.c solib-svr4.c
LIBSRCS+= mips-tdep.c mipsfbsd-tdep.c

nm.h:
	echo '#include "mips/nm-fbsd.h"' > ${.TARGET}

tm.h:
	echo '#include "mips/tm-fbsd.h"' > ${.TARGET}
