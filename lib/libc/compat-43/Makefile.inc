#	@(#)Makefile.inc	8.1 (Berkeley) 6/2/93

# compat-43 sources
.PATH: ${.CURDIR}/${MACHINE}/compat-43 ${.CURDIR}/compat-43

SRCS+=	creat.c gethostid.c getwd.c killpg.c sethostid.c setpgrp.c \
	setregid.c setreuid.c setrgid.c setruid.c sigcompat.c

MAN2+=	creat.0 gethostid.0 killpg.0 setregid.0 setreuid.0 setruid.0 \
	sigblock.0 sigpause.0 sigsetmask.0 sigvec.0

MLINKS+=setruid.2 setrgid.2
MLINKS+=gethostid.2 sethostid.2
