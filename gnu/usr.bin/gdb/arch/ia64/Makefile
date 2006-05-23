# $FreeBSD: src/gnu/usr.bin/gdb/arch/ia64/Makefile,v 1.1 2004/06/25 06:48:53 marcel Exp $

LIBSRCS+= fbsd-proc.c gcore.c
LIBSRCS+= ia64-fbsd-nat.c ia64-fbsd-tdep.c ia64-tdep.c

nm.h:
	echo '#include "ia64/nm-fbsd.h"' > ${.TARGET}

tm.h:
	echo '#include "ia64/tm-fbsd.h"' > ${.TARGET}
