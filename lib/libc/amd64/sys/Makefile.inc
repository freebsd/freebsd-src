#	from: Makefile.inc,v 1.1 1993/09/03 19:04:23 jtc Exp
#	$Id: Makefile.inc,v 1.4 1997/02/22 14:59:27 peter Exp $

.PATH:	${.CURDIR}/../libc/${MACHINE}/sys

SRCS+=	i386_get_ldt.c i386_set_ldt.c

# Only build man pages with libc.
.if ${LIB} == "c"
MAN2+=	i386/sys/i386_get_ldt.2

MLINKS+=i386_get_ldt.2 i386_set_ldt.2
.endif
