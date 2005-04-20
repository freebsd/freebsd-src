# $FreeBSD$

LIBSRCS+= solib-legacy.c
LIBSRCS+= sparc-nat.c sparc-tdep.c sparc64-nat.c sparc64-tdep.c		\
	sparc64fbsd-nat.c sparc64fbsd-tdep.c

nm.h:
	echo '#include "sparc/nm-fbsd.h"' > ${.TARGET}

tm.h:
	echo '#include "sparc/tm-fbsd.h"' > ${.TARGET}
