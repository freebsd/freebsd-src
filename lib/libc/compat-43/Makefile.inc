#	@(#)Makefile.inc	5.3 (Berkeley) 2/20/91

# compat-43 sources
.PATH: ${.CURDIR}/${MACHINE}/compat-43 ${.CURDIR}/compat-43

SRCS+=	creat.c getwd.c killpg.c setpgrp.c sigcompat.c

MAN2+=	compat-43/creat.2 compat-43/killpg.2 compat-43/sigblock.2 \
	compat-43/sigpause.2 compat-43/sigsetmask.2 compat-43/sigvec.2
