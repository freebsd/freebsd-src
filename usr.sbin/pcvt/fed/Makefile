# $FreeBSD$

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
#CFLAGS	=	-g -Wall -pipe
CFLAGS	=	-O -Wall -pipe -I/usr/local/include
OBJS	=	fed.o select.o edit.o misc.o
LIB	=	ncurses
DEST	=	/usr/local/bin

fed:	$(OBJS)
	$(CC) -o fed $(OBJS) -l$(LIB) -L/usr/local/lib

$(OBJS):	fed.h

clean:
	rm -f *.o fed *core* trace*

install:
	${INSTALL} -s -o bin -g bin fed $(DEST)

.endif
