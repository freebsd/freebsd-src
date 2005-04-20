# $FreeBSD$

LIBSRCS+= core-regset.c solib-legacy.c
LIBSRCS+= alpha-mdebug-tdep.c alpha-tdep.c alphabsd-nat.c alphabsd-tdep.c \
	alphafbsd-tdep.c

nm.h:
	echo '#include "alpha/nm-fbsd.h"' > ${.TARGET}

tm.h:
	echo '#include "alpha/tm-alpha.h"' > ${.TARGET}
