#	@(#)Makefile	8.2 (Berkeley) 2/3/94
# $FreeBSD$
#
# All library objects contain rcsid strings by default; they may be
# excluded as a space-saving measure.  To produce a library that does
# not contain these strings, delete -DLIBC_RCS and -DSYSLIBC_RCS
# from CFLAGS below.  To remove these strings from just the system call
# stubs, remove just -DSYSLIBC_RCS from CFLAGS.
LIB=c
SHLIB_MAJOR= 4
SHLIB_MINOR= 0
CFLAGS+=-DLIBC_RCS -DSYSLIBC_RCS -I${.CURDIR}/include
AINC=	-I${.CURDIR}/${MACHINE_ARCH}
CLEANFILES+=tags
INSTALL_PIC_ARCHIVE=	yes
PRECIOUSLIB=	yes

#
# Don't bother hiding any syscalls (like libc_r does).
#
HIDDEN_SYSCALLS=

#
# Include make rules that are shared with libc_r.
#
.include "${.CURDIR}/Makefile.inc"

KQSRCS=	adddi3.c anddi3.c ashldi3.c ashrdi3.c cmpdi2.c divdi3.c iordi3.c \
	lshldi3.c lshrdi3.c moddi3.c muldi3.c negdi2.c notdi2.c qdivrem.c \
	subdi3.c ucmpdi2.c udivdi3.c umoddi3.c xordi3.c
KSRCS=	bcmp.c ffs.c index.c mcount.c rindex.c strcat.c strcmp.c strcpy.c \
	strlen.c strncpy.c

libkern: libkern.gen libkern.${MACHINE_ARCH}

libkern.gen: ${KQSRCS} ${KSRCS}
	cp -p ${.CURDIR}/quad/quad.h ${.ALLSRC} ${DESTDIR}/sys/libkern

libkern.${MACHINE_ARCH}:: ${KMSRCS}
.if defined(KMSRCS) && !empty(KMSRCS)
	cp -p ${.ALLSRC} ${DESTDIR}/sys/libkern/${MACHINE_ARCH}
.endif

.include <bsd.lib.mk>
