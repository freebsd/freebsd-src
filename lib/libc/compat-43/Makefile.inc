#	From: @(#)Makefile.inc	5.3 (Berkeley) 2/20/91
#	$Id: Makefile.inc,v 1.5 1994/04/04 19:33:56 wollman Exp $

# compat-43 sources
.PATH: ${.CURDIR}/${MACHINE}/compat-43 ${.CURDIR}/compat-43

SRCS+=	creat.c getwd.c killpg.c setpgrp.c sigcompat.c setruid.c setrgid.c

MAN2+=	compat-43/creat.2 compat-43/killpg.2 compat-43/sigblock.2 \
	compat-43/sigpause.2 compat-43/sigsetmask.2 compat-43/sigvec.2
