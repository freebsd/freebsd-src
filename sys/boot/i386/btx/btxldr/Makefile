#	$Id: Makefile,v 1.1 1998/09/12 06:30:26 rnordier Exp $

ORG=0x100000

all: btxldr

btxldr: btxldr.o
.if ${OBJFORMAT} == aout
	${LD} -nostdlib -N -s -T ${ORG} -o btxldr.out btxldr.o
	dd if=btxldr.out of=${.TARGET} ibs=32 skip=1
.else
	${LD} -N -e start -Ttext ${ORG} -o btxldr.out btxldr.o
	/usr/libexec/elf/objcopy -S -O binary btxldr.out ${.TARGET}
.endif

btxldr.o: btxldr.s
	${AS} ${AFLAGS} -o ${.TARGET} btxldr.s

clean:
	rm -f btxldr btxldr.out btxldr.o

depend:

install:

