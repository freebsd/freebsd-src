#	@(#)Makefile.inc	8.1 (Berkeley) 6/2/93

# compat-43 sources
.PATH: ${.CURDIR}/../libc/${MACHINE}/compat-43 ${.CURDIR}/../libc/compat-43

SRCS+=	creat.c gethostid.c getwd.c killpg.c sethostid.c setpgrp.c \
	setrgid.c setruid.c sigcompat.c
