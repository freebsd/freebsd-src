# $FreeBSD: src/gnu/usr.bin/gdb/arch/i386/Makefile,v 1.4 2004/12/05 06:59:00 marcel Exp $

GENSRCS+= xm.h
.if !defined(GDB_CROSS_DEBUGGER)
LIBSRCS+= fbsd-proc.c fbsd-threads.c gcore.c
LIBSRCS+= i386-nat.c i386bsd-nat.c i386fbsd-nat.c
.endif
LIBSRCS+= solib.c solib-svr4.c
LIBSRCS+= i386-tdep.c i386bsd-tdep.c i386fbsd-tdep.c i387-tdep.c

nm.h:
	echo '#include "i386/nm-fbsd.h"' > ${.TARGET}

tm.h:
	echo '#include "i386/tm-fbsd.h"' > ${.TARGET}

xm.h:
	echo '#include "i386/xm-i386.h"' > ${.TARGET}
