#	@(#)Makefile	8.2 (Berkeley) 2/3/94
#
# All library objects contain rcsid strings by default; they may be
# excluded as a space-saving measure.  To produce a library that does
# not contain these strings, delete -DLIBC_RCS and -DSYSLIBC_RCS
# from CFLAGS below.  To remove these strings from just the system call
# stubs, remove just -DSYSLIBC_RCS from CFLAGS.
LIB=c_r
SHLIB_MAJOR= 3
SHLIB_MINOR= 0
CFLAGS+=-DLIBC_RCS -DSYSLIBC_RCS
CFLAGS+=-DPTHREAD_KERNEL -D_THREAD_SAFE -I${.CURDIR}/uthread
AINC=	-I${.CURDIR}/../libc/${MACHINE} -I${.CURDIR}/uthread
CLEANFILES+=tags
INSTALL_PIC_ARCHIVE=	yes
PRECIOUSLIB=	yes

.include "${.CURDIR}/db/Makefile.inc"
.include "${.CURDIR}/compat-43/Makefile.inc"
.include "${.CURDIR}/gen/Makefile.inc"
.include "${.CURDIR}/gmon/Makefile.inc"
.include "${.CURDIR}/locale/Makefile.inc"
.include "${.CURDIR}/net/Makefile.inc"
.include "${.CURDIR}/nls/Makefile.inc"
.include "${.CURDIR}/quad/Makefile.inc"
.include "${.CURDIR}/regex/Makefile.inc"
.include "${.CURDIR}/stdio/Makefile.inc"
.include "${.CURDIR}/stdlib/Makefile.inc"
.include "${.CURDIR}/stdtime/Makefile.inc"
.include "${.CURDIR}/string/Makefile.inc"
.include "${.CURDIR}/sys/Makefile.inc"
.include "${.CURDIR}/rpc/Makefile.inc"
.include "${.CURDIR}/uthread/Makefile.inc"
.include "${.CURDIR}/xdr/Makefile.inc"
.if !defined(NO_YP_LIBC)
CFLAGS+= -DYP
.include "${.CURDIR}/yp/Makefile.inc"
.endif
.include "${.CURDIR}/${MACHINE}/sys/Makefile.inc"

KQSRCS=	adddi3.c anddi3.c ashldi3.c ashrdi3.c cmpdi2.c divdi3.c iordi3.c \
	lshldi3.c lshrdi3.c moddi3.c muldi3.c negdi2.c notdi2.c qdivrem.c \
	subdi3.c ucmpdi2.c udivdi3.c umoddi3.c xordi3.c
KSRCS=	bcmp.c ffs.c index.c mcount.c rindex.c strcat.c strcmp.c strcpy.c \
	strlen.c strncpy.c

libkern: libkern.gen libkern.${MACHINE}

libkern.gen: ${KQSRCS} ${KSRCS}
	cp -p ${.CURDIR}/quad/quad.h ${.ALLSRC} /sys/libkern

libkern.${MACHINE}:: ${KMSRCS}
.if defined(KMSRCS) && !empty(KMSRCS)
	cp -p ${.ALLSRC} /sys/libkern/${MACHINE}
.endif

#beforeinstall: tags
#	${INSTALL} ${COPY} -o bin -g bin -m 444 tags /var/db/libc.tags

tags: ${SRCS}
	ctags ${.ALLSRC:M*.c}
	egrep -o "^ENTRY(.*)|^FUNC(.*)|^SYSCALL(.*)" ${.ALLSRC:M*.s} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3 \1 /^\2(\3\4$$/;" \
	    >> tags; sort -o tags tags

.include <bsd.lib.mk>
