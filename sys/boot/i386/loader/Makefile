# $Id: Makefile,v 1.1.1.1 1998/08/21 03:17:41 msmith Exp $
# from	$NetBSD: Makefile,v 1.12 1998/02/19 14:18:36 drochner Exp $

BASE= 		loader
PROG= 		${BASE}.sym
NOMAN=
NEWVERSWHAT=	"bootstrap loader"

# Enable PnP and ISA-PnP code.
#HAVE_PNP=	yes
#HAVE_ISABUS=	yes

# architecture-specific loader code
SRCS+=		main.c conf.c

# Always add MI sources 
.PATH:		${.CURDIR}/../../common
.include	<${.CURDIR}/../../common/Makefile.inc>
CFLAGS+=	-I${.CURDIR}/../../common
# Verbose ls causes extra heap usage
CFLAGS+=	-DVERBOSE_LS

CLEANFILES+=	vers.c vers.o ${BASE}.list

CFLAGS+=	-Wall

# i386 standalone support library
LIBI386=	${.OBJDIR}/../libi386/libi386.a
CFLAGS+=	-I${.CURDIR}/..
CRT=		${.OBJDIR}/../libi386/crt/bootsect/bscrt.o

vers.o:
	sh ${.CURDIR}/newvers.sh ${.CURDIR}/version ${NEWVERSWHAT}
	${CC} -c vers.c

${BASE}.sym: ${OBJS} ${LIBI386} ${CRT} vers.o
	${LD} -o ${BASE}.sym -M -e _start -N -Ttext 0 ${CRT} ${OBJS} \
	vers.o -lstand ${LIBI386} -lstand >${.OBJDIR}/${BASE}.list

.include <bsd.prog.mk>
