# $FreeBSD$

LOADER?=	loader
PROG=		${LOADER}.sym
NEWVERSWHAT=	"bootstrap loader" pc98
VERSION_FILE=	${BOOTSRC}/i386/loader/version

LOADER_NET_SUPPORT=	yes

.include <bsd.init.mk>

# architecture-specific loader code
SRCS=		main.c conf.c vers.c
.PATH:		${BOOTSRC}/i386/loader

# Include bcache code.
HAVE_BCACHE=	yes

# Enable PnP and ISA-PnP code.
HAVE_PNP=	yes
HAVE_ISABUS=	yes

# Always add MI sources
.include	"${BOOTSRC}/loader.mk"

CLEANFILES+=	${LOADER} ${LOADER}.bin loader.help

CFLAGS+=	-Wall
LDFLAGS+=	-static -Ttext 0x0
CFLAGS+=	-I${BOOTSRC} -I${BOOTSRC}/i386

# pc98 standalone support library
LIBPC98=	${BOOTOBJ}/pc98/libpc98/libpc98.a
CFLAGS+=	-I${BOOTSRC}/pc98

# BTX components
CFLAGS+=	-I${.CURDIR}/../btx/lib

# Debug me!
#CFLAGS+=	-g
#LDFLAGS+=	-g

${LOADER}: ${LOADER}.bin ${BTXLDR} ${BTXKERN}
	btxld -v -f aout -e ${LOADER_ADDRESS} -o ${.TARGET} -l ${BTXLDR} \
		-b ${BTXKERN} ${LOADER}.bin

${LOADER}.bin: ${LOADER}.sym
	cp ${.ALLSRC} ${.TARGET}
	strip -R .comment -R .note ${.TARGET}

loader.help: help.common help.pc98
	cat ${.ALLSRC} | awk -f ${BOOTSRC}/common/merge_help.awk > ${.TARGET}

FILES=	${LOADER}
# XXX INSTALLFLAGS_loader= -b
FILESMODE_${LOADER}= ${BINMODE} -b

# XXX crt0.o needs to be first for pxeboot(8) to work
OBJS=	${BTXCRT}

DPADD=	${LDR_INTERP} ${LIBPC98} ${LIBSA}
LDADD=	${LDR_INTERP} ${LIBPC98} ${LIBSA}

.include <bsd.prog.mk>
