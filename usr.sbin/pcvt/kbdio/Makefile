
.if !exists(${.CURDIR}/../Makefile.inc)
error:
	@echo
	@echo "                      You MUST link/copy"
	@echo
	@echo "       ../Makefile.inc.NetBSD    or   ../Makefile.inc.FreeBSD"
	@echo
	@echo "             to ../Makefile.inc before you can proceed !"
	@echo
.else

PROG=	kbdio
SRCS=	kbdio.y lex.l

#YACC=	bison
#YFLAGS+= -yd	# Bison only

YFLAGS+= -v	# verbose
LFLAGS+= -I
LDADD=	-lm -ly -ll

NOMAN=
NOSHARED=	no shared linkage

CFLAGS+= -I ${.CURDIR}/obj -I ${.CURDIR} #-g

CLEANFILES+= y.tab.h
CLEANFILES+= y.output	# comment file from bison

install:
	@echo "kbdio is not installed automatically ...."

.include <bsd.prog.mk>

lex.o: y.tab.h lex.l

.endif
