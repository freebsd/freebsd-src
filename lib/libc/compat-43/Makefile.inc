#	@(#)Makefile.inc	8.1 (Berkeley) 6/2/93
#	$Id$

# compat-43 sources
.PATH: ${.CURDIR}/../libc/${MACHINE}/compat-43 ${.CURDIR}/../libc/compat-43

SRCS+=	creat.c gethostid.c getwd.c killpg.c sethostid.c setpgrp.c \
	setrgid.c setruid.c sigcompat.c

# Only build man pages with libc.
.if ${LIB} == "c"
MAN2+=	compat-43/creat.2 compat-43/killpg.2 \
	compat-43/sigblock.2 \
	compat-43/sigpause.2 compat-43/sigsetmask.2 compat-43/sigvec.2

MAN3+=  compat-43/gethostid.3 compat-43/setruid.3

MLINKS+=setruid.3 setrgid.3
MLINKS+=gethostid.3 sethostid.3
.endif
