# $FreeBSD$

GENSRCS+= xm.h
LIBSRCS+= fbsd-proc.c gcore.c
LIBSRCS+= amd64-nat.c amd64-tdep.c amd64bsd-nat.c amd64fbsd-nat.c	\
	amd64fbsd-tdep.c i386-tdep.c i386bsd-tdep.c i386fbsd-tdep.c	\
	i387-tdep.c

nm.h:
	echo '#include "i386/nm-fbsd64.h"' > ${.TARGET}

tm.h:
	echo '#include "i386/tm-fbsd.h"' > ${.TARGET}

xm.h:
	echo '#include "i386/xm-i386.h"' > ${.TARGET}
