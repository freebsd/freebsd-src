#	@(#)Makefile	7.9 (Berkeley) 6/1/93
# $Id: Makefile,v 1.9 1995/03/23 07:02:14 rgrimes Exp $

.undef NOPROFILE

LIB=	kern
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/.. -DKERNEL

# Stuff we call from the kernel
SRCS+=	mcount.c
SRCS+=	strcpy.c strcmp.c strncpy.c strncmp.c strcat.c
SRCS+=	skpc.c scanc.c random.c inet_ntoa.c

# Stuff GCC generates
SRCS+=	divdi3.c udivdi3.c moddi3.c umoddi3.c 
SRCS+=	qdivrem.c

# Stuff which isn't currently used, if at all..
# SRCS+=	bcmp.c ffs.c locc.c rindex.c strlen.c

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
