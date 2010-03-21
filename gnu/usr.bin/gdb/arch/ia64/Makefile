# $FreeBSD: src/gnu/usr.bin/gdb/arch/ia64/Makefile,v 1.3.24.1 2010/02/10 00:26:20 kensmith Exp $

.if !defined(GDB_CROSS_DEBUGGER)
LIBSRCS+= fbsd-proc.c gcore.c
LIBSRCS+= ia64-fbsd-nat.c
.endif
LIBSRCS+= solib.c solib-svr4.c
LIBSRCS+= ia64-fbsd-tdep.c ia64-tdep.c

nm.h:
	echo '#include "ia64/nm-fbsd.h"' > ${.TARGET}

tm.h:
	echo '#include "ia64/tm-fbsd.h"' > ${.TARGET}
