#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

PROG=	window
SRCS=	char.c cmd.c cmd1.c cmd2.c cmd3.c cmd4.c cmd5.c cmd6.c cmd7.c \
	context.c error.c lcmd.c lcmd1.c lcmd2.c main.c mloop.c parser1.c \
	parser2.c parser3.c parser4.c parser5.c scanner.c startup.c string.c \
	ttf100.c ttgeneric.c tth19.c tth29.c ttinit.c ttoutput.c tttermcap.c \
	tttvi925.c ttwyse60.c ttwyse75.c ttzapple.c ttzentec.c var.c win.c \
	wwadd.c wwalloc.c wwbox.c wwchild.c wwclose.c wwclreol.c wwclreos.c \
	wwcursor.c wwdata.c wwdelchar.c wwdelete.c wwdelline.c wwdump.c \
	wwend.c wwenviron.c wwerror.c wwflush.c wwframe.c wwgets.c wwinit.c \
	wwinschar.c wwinsline.c wwiomux.c wwlabel.c wwmisc.c wwmove.c \
	wwopen.c wwprintf.c wwpty.c wwputc.c wwputs.c wwredraw.c \
	wwredrawwin.c wwrint.c wwscroll.c wwsize.c wwspawn.c wwsuspend.c \
	wwterminfo.c wwtty.c wwunframe.c wwupdate.c wwwrite.c xx.c xxflush.c \
	compress.c
SRCS+=	mystring.h
MAN=	window.1

CFLAGS+= -I.

DPADD=	${LIBTERMCAP}
LDADD=	-ltermcap

mystring.h: string.h
	ln -sf ${.ALLSRC} ${.TARGET}

CLEANFILES+=	mystring.h

.include <bsd.prog.mk>
