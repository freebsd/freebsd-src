# $Id: Makefile,v 1.1 1998/11/03 06:11:34 msmith Exp $
#
LIB=			ficl
NOPROFILE=		yes
INTERNALLIB=		yes
INTERNALSTATICLIB=	yes
SRCS=			dict.c ficl.c math64.c softcore.c stack.c sysdep.c \
			vm.c words.c
CLEANFILES=		softcore.c

# Standard softwords
SOFTWORDS=	softcore.fr jhlocal.fr marker.fr
# Optional OO extension softwords
#SOFTWORDS+=	oo.fr classes.fr

.PATH:		${.CURDIR}/softwords
CFLAGS+=	-I${.CURDIR}

softcore.c:	${SOFTWORDS} softcore.pl
	(cd ${.CURDIR}/softwords; ./softcore.pl ${SOFTWORDS}) > ${.TARGET}

.include <bsd.lib.mk>

