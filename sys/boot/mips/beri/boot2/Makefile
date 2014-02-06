#  $FreeBSD$

PROG=		jtagboot.elf		\
		flashboot.elf

BINDIR?=	/boot
INSTALLFLAGS=	-b

FILES=		${PROG}
SRCS=		relocate.S		\
		start.S			\
		boot2.c			\
		altera_jtag_uart.c	\
		cfi.c			\
		sdcard.c

NO_MAN=

AFLAGS=		-G0

CFLAGS=		-ffreestanding			\
		-I${.CURDIR}			\
		-I${.CURDIR}/../../../common	\
		-I${.CURDIR}/../../../..	\
		-D_KERNEL			\
		-Wall				\
		-G0 -Xassembler -G0		\
		-fno-pic -mno-abicalls		\
		-msoft-float			\
		-g

# where to get libstand from
CFLAGS+=	-I${.CURDIR}/../../../../../lib/libstand/
LIBSTAND=	${.OBJDIR}/../../../../../lib/libstand/libstand.a

LDFLAGS=	-nostdlib			\
		-static				\
		-N				\
		-G0				\
		-L${.CURDIR}

.PATH:		${.CURDIR}/../common
CFLAGS+=	-I${.CURDIR}/../common

flashboot.elf: relocate.o start.o boot2.o altera_jtag_uart.o cfi.o
	${LD} ${LDFLAGS} -T ${.CURDIR}/flashboot.ldscript -o ${.TARGET}	\
	    ${.ALLSRC} ${LIBSTAND}

jtagboot.elf: start.o boot2.o altera_jtag_uart.o cfi.o
	${LD} ${LDFLAGS} -T ${.CURDIR}/jtagboot.ldscript -o ${.TARGET}	\
	    ${.ALLSRC} ${LIBSTAND}

CLEANFILES+=	flashboot.elf jtagboot.elf

.include <bsd.prog.mk>
