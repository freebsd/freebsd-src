#
# $Id: Makefile,v 1.12 1997/02/22 15:44:52 peter Exp $
#

PROG =	cc
MAN1 =  gcc.1
SRCS =	gcc.c
BINDIR=	/usr/bin
SRCS+=	multilib.h obstack.c version.c
LINKS=	${BINDIR}/cc ${BINDIR}/gcc
MLINKS= gcc.1 cc.1 gcc.1 c++.1 gcc.1 g++.1
CLEANFILES+= multilib.h

CFLAGS+= -I.	# I mean it!
CFLAGS+= -I${.CURDIR}

multilib.h: genmultilib
	echo '#define MULTILIB_SELECT "aout maout;elf !maout;"' > multilib.h

.include <bsd.prog.mk>
