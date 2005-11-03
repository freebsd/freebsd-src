# $FreeBSD: src/gnu/usr.bin/gdb/arch/amd64/Makefile,v 1.3 2004/12/05 06:58:59 marcel Exp $

GENSRCS+= xm.h
.if !defined(GDB_CROSS_DEBUGGER)
LIBSRCS+= fbsd-proc.c fbsd-threads.c gcore.c
LIBSRCS+= amd64-nat.c amd64bsd-nat.c amd64fbsd-nat.c
.endif
LIBSRCS+= solib.c solib-svr4.c
LIBSRCS+= amd64-tdep.c amd64fbsd-tdep.c i386-tdep.c i386bsd-tdep.c \
	i386fbsd-tdep.c	i387-tdep.c

nm.h:
	echo '#include "i386/nm-fbsd64.h"' > ${.TARGET}

tm.h:
	echo '#include "i386/tm-fbsd.h"' > ${.TARGET}

xm.h:
	echo '#include "i386/xm-i386.h"' > ${.TARGET}
