#	@(#)Makefile	7.9 (Berkeley) 6/1/93

LIB=	kern
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/..
SRCS=	adddi3.c anddi3.c ashldi3.c ashrdi3.c bcmp.c cmpdi2.c divdi3.c \
	ffs.c iordi3.c locc.c lshldi3.c lshrdi3.c mcount.c moddi3.c \
	muldi3.c negdi2.c notdi2.c qdivrem.c random.c rindex.c scanc.c \
	skpc.c strcat.c strcmp.c strcpy.c strlen.c strncpy.c subdi3.c \
	ucmpdi2.c udivdi3.c umoddi3.c xordi3.c

.if exists(${.CURDIR}/${MACHINE}/Makefile.inc)
.PATH: ${.CURDIR}/${MACHINE}
.include "${.CURDIR}/${MACHINE}/Makefile.inc"
.endif

# mcount cannot be compiled with profiling
mcount.po: mcount.o
	cp mcount.o mcount.po

.include <bsd.lib.mk>
