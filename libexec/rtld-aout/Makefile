#	$Id: Makefile,v 1.26 1998/06/01 13:00:32 peter Exp $
PROG=	ld.so
SRCS=	mdprologue.S rtld.c shlib.c md.c support.c
MAN1=	rtld.1
# As there is relocation going on behind GCC's back, don't cache function addresses.
PICFLAG=-fpic -fno-function-cse
CFLAGS+=-I${.CURDIR} -I${.CURDIR}/${MACHINE_ARCH} ${PICFLAG} -DRTLD -Wall
LDFLAGS+=-nostdlib -Wl,-Bshareable,-Bsymbolic,-assert,nosymbolic
ASFLAGS+=-k
DPADD+=	${LIBC:S/c.a/c_pic.a/} ${LIBC:S/c.a/gcc_pic.a/}
LDADD+=	-lc_pic -lgcc_pic
INSTALLFLAGS+=	-fschg -C	# -C to install as atomically as possible
MLINKS= rtld.1 ld.so.1

.PATH: ${.CURDIR}/${MACHINE_ARCH}

.include <bsd.prog.mk>
