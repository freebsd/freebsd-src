# $Id: Makefile,v 1.13 1998/10/21 20:10:33 msmith Exp $

BASE=		loader
PROG=		${BASE}
NOMAN=
STRIP=
NEWVERSWHAT=	"bootstrap loader"
BINDIR?=	/boot

# architecture-specific loader code
SRCS=		main.c conf.c

# Enable PnP and ISA-PnP code.
HAVE_PNP=	yes
HAVE_ISABUS=	yes

# Always add MI sources 
.PATH:		${.CURDIR}/../../common
.include	<${.CURDIR}/../../common/Makefile.inc>
CFLAGS+=	-I${.CURDIR}/../../common
CFLAGS+=	-I${.CURDIR}/../../.. -I.

CLEANFILES+=	vers.c vers.o ${BASE}.list ${BASE}.bin

CFLAGS+=	-Wall
LDFLAGS=	-nostdlib -static -Ttext 0x1000

# i386 standalone support library
LIBI386=	${.OBJDIR}/../libi386/libi386.a
CFLAGS+=	-I${.CURDIR}/..

# where to get libstand from
LIBSTAND=	-lstand
#LIBSTAND=	${.CURDIR}/../../../lib/libstand/libstand.a
#CFLAGS+=	-I${.CURDIR}/../../../lib/libstand/

# BTX components
.if exists(${.OBJDIR}/../btx)
BTXDIR=		${.OBJDIR}/../btx
.else
BTXDIR=		${.CURDIR}/../btx
.endif
BTXLDR=		${BTXDIR}/btxldr/btxldr
BTXKERN=	${BTXDIR}/btx/btx
BTXCRT=		${BTXDIR}/lib/crt0.o
CFLAGS+=	-I${.CURDIR}/../btx/lib

# BTX is expecting ELF components
CFLAGS+=	-elf

# New linker set code
CFLAGS+=	-DNEW_LINKER_SET

# Debug me!
#CFLAGS+=	-g
#LDFLAGS+=	-g

vers.o:
	sh ${.CURDIR}/newvers.sh ${.CURDIR}/version ${NEWVERSWHAT}
	${CC} -c vers.c

${BASE}: ${BASE}.bin ${BTXLDR} ${BTXKERN} ${BTXCRT}
	btxld -v -f aout -e 0x100000 -o ${.TARGET} -l ${BTXLDR} -b ${BTXKERN} \
		${BASE}.bin

${BASE}.bin: ${BASE}.sym
	cp ${.ALLSRC} ${.TARGET}
	strip ${.TARGET}

# Cannot use ${OBJS} above this line
.include <bsd.prog.mk>

${BASE}.sym: ${OBJS} ${LIBI386} vers.o
	${CC} ${LDFLAGS} -o ${.TARGET} ${BTXCRT} ${OBJS} vers.o \
		${LIBSTAND} ${LIBI386} ${LIBSTAND}

# If it's not there, don't consider it a target
.if exists(${.CURDIR}/../../../i386/include)
beforedepend ${OBJS}: machine

machine:
	ln -sf ${.CURDIR}/../../../i386/include machine

.endif

CLEANFILES+=	machine


