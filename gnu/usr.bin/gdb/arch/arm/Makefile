# $FreeBSD: src/gnu/usr.bin/gdb/arch/arm/Makefile,v 1.1.2.1.4.1 2007/12/02 14:38:21 cognet Exp $

GENSRCS+= xm.h
LIBSRCS+= armfbsd-nat.c
LIBSRCS+= arm-tdep.c armfbsd-tdep.c solib.c solib-svr4.c fbsd-threads.c

nm.h:
#XXX this should be arm/nm-fbsd.h but won't until it's merged into the gdb repo
	echo '#include "nm-fbsd.h"' > ${.TARGET}

tm.h:
#XXX this should be arm/tm-fbsd.h
	echo '#include "tm-fbsd.h"' > ${.TARGET}

xm.h:
#XXX this should be arm/xm-fbsd.h
	echo '#include "xm-fbsd.h"' > ${.TARGET}
