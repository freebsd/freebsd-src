# $FreeBSD$

CFLAGS=-O2 -mcpu=arm9 -ffreestanding \
	-I${.CURDIR}/../libat91 \
	-Wall -Waggregate-return -Wcast-align \
	-Wmissing-declarations -Wmissing-prototypes -Wnested-externs \
	-Wpointer-arith -Wshadow -Wstrict-prototypes -Wwrite-strings \
	-Werror

CFLAGS+=-DBOOT_TSC
#CFLAGS+=-DBOOT_KB920X

LIBAT91=${.OBJDIR}/../libat91/libat91.a

.if defined(P)
${P}:	${P}.out
	objcopy -S -O binary ${P}.out ${.TARGET}

${P}.out: ${OBJS}
	ld ${LDFLAGS} -o ${.TARGET} ${OBJS} ${LIBAT91}

CLEANFILES+= ${P} ${P}.out
.endif
