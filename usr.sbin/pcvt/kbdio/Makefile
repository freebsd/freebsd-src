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
SRCS=	kbdio.c lex.c

#YACC=	bison
#YFLAGS+= -yd	# Bison only

YFLAGS+= -v	# verbose

LFLAGS+= -I

DPADD=	${LIBM} ${LIBY} ${LIBL}
LDADD=	-lm -ly -ll

NOMAN=

CFLAGS+= -I${.OBJDIR} -I${.CURDIR} #-g

CLEANFILES+= kbdio.c lex.c lex.yy.c y.tab.[ch]
CLEANFILES+= y.output	# comment file from bison

install:
	@${ECHO} "kbdio is not installed automatically ...."

.include <bsd.prog.mk>

.endif
