#	@(#)Makefile	7.9 (Berkeley) 6/1/93
# $Id: Makefile,v 1.11 1995/11/08 08:40:09 phk Exp $

.undef NOPROFILE

LIB=	kern
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/.. -DKERNEL

# Stuff we call from the kernel
SRCS+=	inet_ntoa.c mcount.c qsort.c
SRCS+=	random.c scanc.c skpc.c
SRCS+=	strcat.c strcmp.c strcpy.c strlen.c strncmp.c strncpy.c

# Stuff GCC generates
SRCS+=	divdi3.c udivdi3.c moddi3.c umoddi3.c 
SRCS+=	qdivrem.c

# Stuff which isn't currently used, if at all..
# SRCS+=	bcmp.c ffs.c locc.c rindex.c

# Stuff GCC may need....
# SRCS+=	adddi3.c anddi3.c ashldi3.c ashrdi3.c cmpdi2.c 
# SRCS+=	iordi3.c lshldi3.c lshrdi3.c muldi3.c negdi2.c notdi2.c 
# SRCS+=	subdi3.c ucmpdi2.c xordi3.c

.if exists(${.CURDIR}/${MACHINE}/Makefile.inc)
.PATH: ${.CURDIR}/${MACHINE}
.include "${.CURDIR}/${MACHINE}/Makefile.inc"
.endif

# mcount cannot be compiled with profiling
mcount.po: mcount.o
	cp mcount.o mcount.po

install:

.include <bsd.lib.mk>
