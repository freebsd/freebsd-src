# $FreeBSD$

BOOTBLOCKBASE=	0x4000

CFLAGS=	-W -Wall -I../../ -I../../common/ -Os \
	-DBOOTBLOCKBASE=${BOOTBLOCKBASE} \
	-ffreestanding -mno-app-regs -mcmodel=medlow
OBJ=	bootblock.o

all:	bootblock

bootblock.o:	bootblock.c
	${CC} ${CFLAGS} -c -o ${.TARGET} ${.ALLSRC}

bootblock:	${OBJ}
	${LD} -N -Ttext ${BOOTBLOCKBASE} -e main -o bootblock ${OBJ}
	/usr/local/bin/elftoaout bootblock
clean:
	rm -f *.o bootblock
