#
# $Id: Makefile,v 1.3 1998/03/12 05:14:16 jdp Exp $
#

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/binutils

LIB=		binutils
SRCS+=		arlex.l arparse.y arsup.c bucomm.c debug.c filemode.c \
		ieee.c rdcoff.c rddbg.c stabs.c wrstabs.c version.c
CFLAGS+=	-DTARGET=\"${MACHINE_ARCH}-unknown-freebsdelf\"
CFLAGS+=	-DVERSION=\"${VERSION}\"
CFLAGS+=	-I${SRCDIR}/binutils
CFLAGS+=	-I${SRCDIR}/bfd
NOPROFILE=	true
NOPIC=		true
INTERNALLIB=	true
INTERNALSTATICLIB=true
CLEANFILES+=	arlex.c arparse.c arparse.h

beforedepend: arlex.c arparse.c arparse.h

arlex.o: arparse.h

arparse.c arparse.h: arparse.y
	${YACC} -d -o arparse.c ${.IMPSRC}

# This rule is an attempt to cause "make -j" to work.
arparse.h: arparse.c

.include <bsd.lib.mk>
