#	from: Makefile.inc,v 1.1 1993/09/03 19:04:23 jtc Exp
#	$Id: Makefile.inc,v 1.2 1994/02/21 05:19:06 rgrimes Exp $

.PATH:	${.CURDIR}/../libc/${MACHINE}/sys

SRCS+=	i386_get_ldt.c i386_set_ldt.c
