#	@(#)Makefile.inc	8.1 (Berkeley) 6/2/93
# $FreeBSD: src/lib/libc/compat-43/Makefile.inc,v 1.9 1999/08/27 23:58:12 peter Exp $

# compat-43 sources
.PATH: ${.CURDIR}/../libc/${MACHINE_ARCH}/compat-43 ${.CURDIR}/../libc/compat-43

SRCS+=	creat.c gethostid.c getwd.c killpg.c sethostid.c setpgrp.c \
	setrgid.c setruid.c sigcompat.c

.if ${LIB} == "c"
MAN2+=	creat.2 killpg.2 sigblock.2 sigpause.2 sigsetmask.2 sigvec.2
MAN3+=	gethostid.3 setruid.3

MLINKS+=gethostid.3 sethostid.3
MLINKS+=setruid.3 setrgid.3
.endif
