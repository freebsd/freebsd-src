#	from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
#	$Id: Makefile.inc,v 1.5 1997/05/03 03:50:02 jb Exp $

# Quad support, if needed
.PATH: ${.CURDIR}/../libc/${MACHINE}/quad ${.CURDIR}/../libc/quad

.if ${MACHINE} == "i386"

SRCS+=	cmpdi2.c divdi3.c moddi3.c qdivrem.c ucmpdi2.c udivdi3.c umoddi3.c

.else

SRCS+=	adddi3.c anddi3.c ashldi3.c ashrdi3.c cmpdi2.c divdi3.c fixdfdi.c \
	fixsfdi.c fixunsdfdi.c fixunssfdi.c floatdidf.c floatdisf.c \
	floatunsdidf.c iordi3.c lshldi3.c lshrdi3.c moddi3.c muldi3.c \
	negdi2.c notdi2.c qdivrem.c subdi3.c ucmpdi2.c udivdi3.c umoddi3.c \
	xordi3.c

.endif
