#	from: Makefile.inc,v 1.1 1993/09/03 19:04:23 jtc Exp
#	$Id: Makefile.inc,v 1.5 1997/05/03 03:49:56 jb Exp $

.PATH:	${.CURDIR}/../libc/${MACHINE}/sys

SRCS+=	i386_get_ldt.c i386_set_ldt.c

.if ${LIB} == "c"
MAN2+=	i386_get_ldt.2

MLINKS+=i386_get_ldt.2 i386_set_ldt.2
.endif
