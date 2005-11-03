#	@(#)Makefile.inc	8.1 (Berkeley) 6/2/93
# $FreeBSD: src/lib/libc/compat-43/Makefile.inc,v 1.12 2002/11/18 09:50:54 ru Exp $

# compat-43 sources
.PATH: ${.CURDIR}/${MACHINE_ARCH}/compat-43 ${.CURDIR}/compat-43

SRCS+=	creat.c gethostid.c getwd.c killpg.c sethostid.c setpgrp.c \
	setrgid.c setruid.c sigcompat.c

MAN+=	creat.2 killpg.2 sigpause.2 sigsetmask.2 sigvec.2
MAN+=	gethostid.3 setruid.3

MLINKS+=gethostid.3 sethostid.3
MLINKS+=setruid.3 setrgid.3

MLINKS+=sigsetmask.2 sigblock.2
MLINKS+=sigsetmask.2 sigmask.2
