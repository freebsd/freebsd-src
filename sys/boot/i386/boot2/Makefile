#	$Id:$

M4?=	m4

.if exists(${.OBJDIR}/../btx)
BTX=	${.OBJDIR}/../btx
.else
BTX=	${.CURDIR}/../btx
.endif

ORG1=	0x7c00
ORG2=	0x1000

CFLAGS=	-elf -I${BTX}/lib -I. -fno-builtin \
	-O2 -malign-functions=0 -malign-jumps=0 -malign-loops=0 -mrtd \
	-Wall -Waggregate-return -Wbad-function-cast -Wcast-align \
	-Wmissing-declarations -Wmissing-prototypes -Wnested-externs \
	-Wpointer-arith -Wshadow -Wstrict-prototypes -Wwrite-strings

LDFLAGS=-nostdlib -static -N

all: boot1 boot2

boot1: boot1.out
	/usr/libexec/elf/objcopy -S -O binary boot1.out ${.TARGET}

boot1.out: boot1.o
	${LD} ${LDFLAGS} -e start -Ttext ${ORG1} -o ${.TARGET} boot1.o

boot1.o: boot1.m4 boot1.s
	${M4} boot1.m4 boot1.s | ${AS} ${AFLAGS} -o ${.TARGET}

boot2: boot2.ldr boot2.bin
	btxld -v -E ${ORG2} -f bin -b ${BTX}/btx/btx -l boot2.ldr \
		-o ${.TARGET} -P 1 boot2.bin
	@ls -l boot2 | awk '{ x = 7680 - $$5; print x " bytes free"; \
		if (x < 0) exit 1 }'

boot2.ldr:
	dd if=/dev/zero of=${.TARGET} bs=512 count=1 2>/dev/null

boot2.bin: boot2.out
	/usr/libexec/elf/objcopy -S -O binary boot2.out ${.TARGET}

boot2.out: boot2.o
	${LD} ${LDFLAGS} -Ttext ${ORG2} -o ${.TARGET} \
		${BTX}/lib/crt0.o boot2.o

clean:
	rm -f boot1 boot1.out boot1.o boot2 boot2.ldr boot2.bin \
		boot2.out boot2.o
