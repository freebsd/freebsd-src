#	@(#)Makefile.inc	8.1 (Berkeley) 6/2/93
# $FreeBSD$

# compat-43 sources
.PATH: ${.CURDIR}/../libc/${MACHINE_ARCH}/compat-43 ${.CURDIR}/../libc/compat-43

SRCS+=	creat.c gethostid.c getwd.c killpg.c sethostid.c setpgrp.c \
	setrgid.c setruid.c sigcompat.c

.if ${LIB} == "c"
MAN+=	creat.2 killpg.2 sigpause.2 sigsetmask.2 sigvec.2
MAN+=	gethostid.3 setruid.3

MLINKS+=gethostid.3 sethostid.3
MLINKS+=setruid.3 setrgid.3

MLINKS+=sigsetmask.2 sigblock.2
MLINKS+=sigsetmask.2 sigmask.2
.endif
