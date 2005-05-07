# $FreeBSD: src/gnu/usr.bin/gdb/arch/i386/Makefile,v 1.1 2004/06/25 06:48:53 marcel Exp $

GENSRCS+= xm.h
LIBSRCS+= fbsd-proc.c gcore.c
LIBSRCS+= i386-nat.c i386-tdep.c i386bsd-nat.c i386bsd-tdep.c		\
	i386fbsd-nat.c	 i386fbsd-tdep.c i387-tdep.c

nm.h:
	echo '#include "i386/nm-fbsd.h"' > ${.TARGET}

tm.h:
	echo '#include "i386/tm-fbsd.h"' > ${.TARGET}

xm.h:
	echo '#include "i386/xm-i386.h"' > ${.TARGET}
