# $FreeBSD: src/gnu/usr.bin/gdb/arch/mips/Makefile,v 1.1.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

.if !defined(GDB_CROSS_DEBUGGER)
LIBSRCS+= mips-nat.c mips-nat.c mipsfbsd-nat.c
.endif
LIBSRCS+= solib.c solib-svr4.c
LIBSRCS+= mips-tdep.c mipsfbsd-tdep.c

nm.h:
	echo '#include "mips/nm-fbsd.h"' > ${.TARGET}

tm.h:
	echo '#include "mips/tm-fbsd.h"' > ${.TARGET}
