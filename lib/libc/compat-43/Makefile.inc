#	@(#)Makefile.inc	8.1 (Berkeley) 6/2/93
#	$Id: Makefile.inc,v 1.5 1997/05/03 03:49:51 jb Exp $

# compat-43 sources
.PATH: ${.CURDIR}/../libc/${MACHINE}/compat-43 ${.CURDIR}/../libc/compat-43

SRCS+=	creat.c gethostid.c getwd.c killpg.c sethostid.c setpgrp.c \
	setrgid.c setruid.c sigcompat.c

.if ${LIB} == "c"
MAN2+=	creat.2 killpg.2 sigblock.2 sigpause.2 sigsetmask.2 sigvec.2

MAN3+=  gethostid.3 setruid.3

MLINKS+=setruid.3 setrgid.3
MLINKS+=gethostid.3 sethostid.3
.endif
