# Address at which ubldr will be loaded.
# This varies for different boards and SOCs.
UBLDR_LOADADDR?=	0x1000000

SRCS+=		start.S conf.c self_reloc.c 

abs.ldscript:
	echo "UBLDR_LOADADDR = ${UBLDR_LOADADDR};" >${.TARGET}

pie.ldscript:
	echo "UBLDR_LOADADDR = 0;" >${.TARGET}

ubldr: ${OBJS} abs.ldscript ${LDSCRIPT} ${DPADD}
	${CC} ${CFLAGS} -T abs.ldscript ${LDFLAGS} \
	    -o ${.TARGET} ${OBJS} ${LDADD}

ubldr.pie: ${OBJS} pie.ldscript ${LDSCRIPT} ${DPADD}
	${CC} ${CFLAGS} -T pie.ldscript ${LDFLAGS} -pie -Wl,-Bsymbolic \
	    -o ${.TARGET} ${OBJS} ${LDADD}

ubldr.bin: ubldr.pie
	${OBJCOPY} -S -O binary ubldr.pie ${.TARGET}

CLEANFILES+=	abs.ldscript pie.ldscript ubldr ubldr.pie ubldr.bin

