# Address at which ubldr will be loaded.
# This varies for different boards and SOCs.
UBLDR_LOADADDR?=	0x1000000

SRCS+=		start.S conf.c self_reloc.c 

ldscript.abs:
	echo "UBLDR_LOADADDR = ${UBLDR_LOADADDR};" >${.TARGET}

ldscript.pie:
	echo "UBLDR_LOADADDR = 0;" >${.TARGET}

ubldr: ${OBJS} ldscript.abs ${LDSCRIPT} ${DPADD}
	${CC} ${CFLAGS} -T ldscript.abs ${LDFLAGS} \
	    -o ${.TARGET} ${OBJS} ${LDADD}

ubldr.pie: ${OBJS} ldscript.pie ${LDSCRIPT} ${DPADD}
	${CC} ${CFLAGS} -T ldscript.pie ${LDFLAGS} -pie -Wl,-Bsymbolic \
	    -o ${.TARGET} ${OBJS} ${LDADD}

ubldr.bin: ubldr.pie
	${OBJCOPY} -S -O binary ubldr.pie ${.TARGET}

CLEANFILES+=	ldscript.abs ldscript.pie ubldr ubldr.pie ubldr.bin

