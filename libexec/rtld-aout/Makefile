# $FreeBSD: src/libexec/rtld-aout/Makefile,v 1.29 2000/01/29 03:13:49 jdp Exp $
PROG=	ld.so
SRCS=	mdprologue.S rtld.c shlib.c md.c support.c
MAN1aout=	rtld.1aout
# As there is relocation going on behind GCC's back, don't cache function addresses.
PICFLAG=-fpic -fno-function-cse
CFLAGS+=-I${.CURDIR} -I${.CURDIR}/${MACHINE_ARCH} ${PICFLAG} -DRTLD -Wall
LDFLAGS+=-nostdlib -Wl,-Bshareable,-Bsymbolic,-assert,nosymbolic
ASFLAGS+=-k
DPADD+=	${LIBC:S/c.a/c_pic.a/} ${LIBC:S/c.a/gcc_pic.a/}
LDADD+=	-lc_pic -lgcc_pic
INSTALLFLAGS+=	-fschg -C	# -C to install as atomically as possible
MLINKS= rtld.1aout ld.so.1aout

.PATH: ${.CURDIR}/${MACHINE_ARCH}

.include <bsd.prog.mk>
