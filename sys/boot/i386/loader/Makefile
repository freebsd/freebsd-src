# $Id: Makefile,v 1.2 1998/09/14 18:27:06 msmith Exp $
# from	$NetBSD: Makefile,v 1.12 1998/02/19 14:18:36 drochner Exp $

BASE=		loader
PROG=		${BASE}
NOMAN=
NEWVERSWHAT=	"bootstrap loader"

# architecture-specific loader code
SRCS=		main.c conf.c

# Enable PnP and ISA-PnP code.
#HAVE_PNP=	yes
#HAVE_ISABUS=	yes

# Verbose ls causes extra heap usage
CFLAGS+=	-DVERBOSE_LS

# Always add MI sources 
.PATH:		${.CURDIR}/../../common
.include	<${.CURDIR}/../../common/Makefile.inc>
CFLAGS+=	-I${.CURDIR}/../../common

CLEANFILES+=	vers.c vers.o ${BASE}.list setdef0.o setdef1.o setdefs.h \
		gensetdefs.o gensetdefs ${BASE}.bin

CFLAGS+=	-Wall
LDFLAGS=	-nostdlib -static -Ttext 0x1000
LDADD=		-lstand ${LIBI386}

# i386 standalone support library
LIBI386=	${.OBJDIR}/../libi386/libi386.a
CFLAGS+=	-I${.CURDIR}/..

# BTX components
BTXDIR=		${.CURDIR}/../btx
BTXLDR=		${BTXDIR}/btxldr/btxldr
BTXKERN=	${BTXDIR}/btx/btx
BTXCRT=		${BTXDIR}/lib/crt0.o
CFLAGS+=	-I${BTXDIR}/lib

# BTX is expecting ELF components
CFLAGS+=	-elf

vers.o:
	sh ${.CURDIR}/newvers.sh ${.CURDIR}/version ${NEWVERSWHAT}
	${CC} -c vers.c

${BASE}: ${BASE}.bin ${BTXLDR} ${BTXKERN}
	btxld -v -f aout -e 0x100000 -o ${.TARGET} -l ${BTXLDR} -b ${BTXKERN} ${BASE}.bin

${BASE}.bin: ${OBJS} ${LIBI386} vers.o setdef0.o setdef1.o
	${LD} ${LDFLAGS} -o ${.TARGET} ${BTXCRT} setdef0.o ${OBJS} vers.o setdef1.o -lstand ${LIBI386}

setdef0.o:	setdefs.h

setdef1.o:	setdefs.h

.include <bsd.prog.mk>

# Linker set gymnastics
setdefs.h: gensetdefs ${OBJS}
	@echo Generating linker sets
	@./gensetdefs ${OBJS} >setdefs.h

gensetdefs: gensetdefs.o
	${CC} -static gensetdefs.o -o $@

gensetdefs.o: gensetdefs.c
	${CC} -c $<


