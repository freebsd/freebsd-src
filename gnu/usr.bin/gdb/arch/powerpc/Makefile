# $FreeBSD: src/gnu/usr.bin/gdb/arch/powerpc/Makefile,v 1.2.6.1 2008/11/25 02:59:29 kensmith Exp $

.if !defined(GDB_CROSS_DEBUGGER)
LIBSRCS+= fbsd-proc.c fbsd-threads.c gcore.c
LIBSRCS+= ppcfbsd-nat.c
.endif
LIBSRCS+= solib.c solib-svr4.c
LIBSRCS+= ppc-sysv-tdep.c ppcfbsd-tdep.c rs6000-tdep.c

nm.h:
	echo '#include "powerpc/nm-fbsd.h"' > ${.TARGET}

tm.h:
	echo '#include "powerpc/tm-ppc-eabi.h"' > ${.TARGET}
