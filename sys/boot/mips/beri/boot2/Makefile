#  $FreeBSD$

BINDIR?=	/boot
INSTALLFLAGS=	-b

LOADERS=	flashboot jtagboot
FILES=		${LOADERS} ${LOADERS:S/$/.md5/}

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

flashboot.elf: relocate.o start.o boot2.o altera_jtag_uart.o cfi.o sdcard.o
	${LD} ${LDFLAGS} -T ${.CURDIR}/flashboot.ldscript -o ${.TARGET}	\
	    ${.ALLSRC} ${LIBSTAND}
flashboot: flashboot.elf
	objcopy -S -O binary ${.TARGET}.elf ${.TARGET}
flashboot.md5: flashboot
	md5 flashboot > flashboot.md5

jtagboot: start.o boot2.o altera_jtag_uart.o cfi.o sdcard.o
	${LD} ${LDFLAGS} -T ${.CURDIR}/jtagboot.ldscript -o ${.TARGET}	\
	    ${.ALLSRC} ${LIBSTAND}
jtagboot.md5: jtagboot
	md5 jtagboot > jtagboot.md5

CLEANFILES+=	flashboot.elf

.include <bsd.prog.mk>
