#
# $Id: Makefile,v 1.9 1996/02/09 16:18:56 mpp Exp $
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

# I have NO idea what this is for, it appears to be m68k specific.
multilib.h: genmultilib
	sh ${GCCDIR}/genmultilib "$(MULTILIB_OPTIONS)" \
		"$(MULTILIB_DIRNAMES)" "$(MULTILIB_MATCHES)" > multilib.h

.include <bsd.prog.mk>
