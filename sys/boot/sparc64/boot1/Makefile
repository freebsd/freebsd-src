# $FreeBSD$

BOOTBLOCKBASE=	0x4000

CFLAGS=	-W -Wall -I../../ -I../../common/ -Os \
	-DBOOTBLOCKBASE=${BOOTBLOCKBASE} \
	-ffreestanding -mno-app-regs -mcmodel=medlow
OBJ=	boot1.o

all:	boot1

boot1.o:	boot1.c
	${CC} ${CFLAGS} -c -o ${.TARGET} ${.ALLSRC}

boot1:	${OBJ}
	${LD} -N -Ttext ${BOOTBLOCKBASE} -e main -o boot1 ${OBJ}
	/usr/local/bin/elftoaout boot1
clean:
	rm -f *.o boot1
