
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

PROG=	vgaio
MAN8=	vgaio.${MAN8EXT}

SRCS=	vgaio.y lex.l
YACC=	yacc

#YFLAGS+= -yd	# Bison
#YFLAGS+= -v	# verbose

LFLAGS+= -I
LDADD=	-lm -ly -ll

CFLAGS+= -I ${.CURDIR}/obj -I ${.CURDIR} -g

CLEANFILES+= y.tab.h
CLEANFILES+= y.output	# comment file from bison

install:
	@echo "vgaio is not installed automatically ...."

.include <bsd.prog.mk>

.endif
