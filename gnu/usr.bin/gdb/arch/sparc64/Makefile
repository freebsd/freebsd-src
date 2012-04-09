# $FreeBSD: src/gnu/usr.bin/gdb/arch/sparc64/Makefile,v 1.3.22.2.2.1 2012/03/03 06:15:13 kensmith Exp $

.if !defined(GDB_CROSS_DEBUGGER)
LIBSRCS+= fbsd-threads.c
LIBSRCS+= solib.c solib-legacy.c solib-svr4.c
LIBSRCS+= sparc-nat.c sparc64-nat.c sparc64fbsd-nat.c
.endif
LIBSRCS+= sparc-tdep.c sparc64-tdep.c sparc64fbsd-tdep.c

nm.h:
	echo '#include "sparc/nm-fbsd.h"' > ${.TARGET}

tm.h:
	echo '#include "sparc/tm-fbsd.h"' > ${.TARGET}
