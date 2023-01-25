# Address at which ubldr will be loaded.
# This varies for different boards and SOCs.
.if ${MACHINE_ARCH:Mmips64*}
UBLDR_LOADADDR?=	0xffffffff80800000
.else
UBLDR_LOADADDR?=	0x80800000
.endif

# Architecture-specific loader code
SRCS+=		start.S conf.c

# ubldr on MIPS is built with '-mno-abicalls' flag,
# which is incompatible with PIE
MK_PIE=		no

ldscript.abs:
	echo "UBLDR_LOADADDR = ${UBLDR_LOADADDR};" >${.TARGET}

ubldr: ${OBJS} ldscript.abs ${LDSCRIPT} ${DPADD}
	${CC} ${CFLAGS} -T ldscript.abs ${LDFLAGS} \
	    -o ${.TARGET} ${OBJS} ${LDADD}

ubldr.bin: ubldr
	${OBJCOPY} -S -O binary ubldr ubldr.bin

CLEANFILES+=	ldscript.abs ubldr ubldr.pie ubldr.bin
