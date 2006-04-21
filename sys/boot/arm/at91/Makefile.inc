# $FreeBSD$

.if !target(__at91_boot_Makefile.inc__)
__at91_boot_Makefile.inc__:

CFLAGS=-O2 -mcpu=arm9 -ffreestanding \
	-I${.CURDIR}/../libat91 \
	-Wall -Waggregate-return -Wcast-align \
	-Wmissing-declarations -Wmissing-prototypes -Wnested-externs \
	-Wpointer-arith -Wshadow -Wstrict-prototypes -Wwrite-strings \
	-Werror

#CFLAGS+=-DBOOT_TSC
CFLAGS+=-DBOOT_KB9202

LIBAT91=${.OBJDIR}/../libat91/libat91.a

.if defined(P)
${P}:	${P}.out
	objcopy -S -O binary ${P}.out ${.TARGET}
	@set -- `ls -l ${.TARGET}`; x=$$((8192-$$5)); \
	    echo "$$x bytes available"; test $$x -ge 0

${P}.out: ${OBJS}
	ld ${LDFLAGS} -o ${.TARGET} ${OBJS} ${LIBAT91}

CLEANFILES+= ${P} ${P}.out
.endif

.if defined(WITH_TAG_LIST)
MK_TAG_LIST:=yes
.else
MK_TAG_LIST:=no
.endif

.endif
