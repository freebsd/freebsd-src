PROG=		kcon
DEVICE=		/dev/ttyv0
CFLAGS+= 	-I${.CURDIR}/../keycap -DKEYB_DEVICE=\"${DEVICE}\"

.if exists(${.OBJDIR}/../keycap)
LIBDESTDIR=	${.OBJDIR}/../keycap
.else
LIBDESTDIR=	${.CURDIR}/../keycap
.endif

# the -Lfoo could be omitted if libkeycap.a were installed before
# making those programs here

DPADD =       ${LIBDESTDIR}/libkeycap.a
LDADD =       -L${LIBDESTDIR} -lkeycap

.include <bsd.prog.mk>
