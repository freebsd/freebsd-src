# $FreeBSD: src/gnu/usr.bin/gdb/arch/powerpc/Makefile,v 1.2.10.1.6.1 2010/12/21 17:09:25 kensmith Exp $

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
