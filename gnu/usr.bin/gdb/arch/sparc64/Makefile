# $FreeBSD: src/gnu/usr.bin/gdb/arch/sparc64/Makefile,v 1.1 2004/06/25 06:48:54 marcel Exp $

LIBSRCS+= solib-legacy.c
LIBSRCS+= sparc-nat.c sparc-tdep.c sparc64-nat.c sparc64-tdep.c		\
	sparc64fbsd-nat.c sparc64fbsd-tdep.c

nm.h:
	echo '#include "sparc/nm-fbsd.h"' > ${.TARGET}

tm.h:
	echo '#include "sparc/tm-fbsd.h"' > ${.TARGET}
