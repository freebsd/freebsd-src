#	@(#)Makefile	8.1 (Berkeley) 6/19/93

PROG=	routed
SRCS=	if.c input.c main.c output.c parms.c radix.c rdisc.c table.c trace.c
MAN8=	routed.0
SUBDIR=	rtquery
DPADD=	${LIBCOMPAT}
LDADD=	-lcompat
#COPTS=	-g -DDEBUG -Wall

.include <bsd.prog.mk>

.if (${MACHINE} == "vax")
# The following can be deleted where not appropriate to use the kernel's
# inline code expansions.
INLINE=	/sys/vax/inline/obj/inline
C2=	/usr/libexec/c2
.c.o:
	${CC} -S ${CFLAGS} ${.CURDIR}/${.PREFIX}.c
	@${C2} ${.PREFIX}.s | ${INLINE} | ${AS} -o ${.PREFIX}.o
	@rm -f ${.PREFIX}.s
.endif
