# RCS Info: $Revision: 1.1.1.1 $ on $Date: 1993/06/12 14:41:05 $
#           $Source: /a/cvs/386BSD/src/gnu/chess/Xchess/Makefile,v $
# Copyright (c) 1985 Wayne A. Christopher, U. C. Berkeley CAD Group
#
# Makefile for xchess.

#CC = cc -O -m68010 -L/pub.MC68010/lib -L/usr.MC68010/lib
CC = cc -O

CFILES	=\
	board.c\
	button.c\
	clock.c\
	control.c\
	jail.c\
	message.c\
	parse.c\
	popup.c\
	program.c\
	record.c\
	std.c\
	valid.c\
	window.c\
	XCircle.c

COBJS	=\
	board.o\
	button.o\
	clock.o\
	control.o\
	jail.o\
	message.o\
	parse.o\
	popup.o\
	program.o\
	record.o\
	std.o\
	valid.o\
	window.o\
	XCircle.o

HFILES	=\
	std.h\
	xchess.h

SOURCE = $(CFILES)

ALLFILES = $(SOURCE) $(HFILES)

INCLUDE = -I. -I/usr/X386/include

DEFINES = -DDEF_PROGRAM=\"/usr/bin/chess\"

CFLAGS = $(DEFINES) $(INCLUDE)
LINTFLAGS = -u -z -lc -DLINT $(DEFINES) $(INCLUDE)
LINTLIB = ../lib/llib-lX.ln
#LDFLAGS = -L/usr2/X/lib -z -lX -lm
LDFLAGS = -L/usr/X386/lib -loldX -lX11 -z -lm
GPLDFLAGS = -z -loldX -lX11 -lXMenu_p -lX -lm_p -g -pg

.c.o: $*.c
	$(CC) $(CFLAGS) -c $*.c
.s.o: $*.s
	$(CC) $(CFLAGS) -c $*.s

all: xchess scrollText.o
	@echo "All done."

everything: all tags depend lint wc
	@echo "All done."

xchess:	xchess.o $(COBJS) scrollText.o
	$(CC) -o xchess xchess.o $(COBJS) scrollText.o \
	$(LDFLAGS)

scrollText.o: scrollText.h scrollText.c

gpxchess: xchess.o $(COBJS)
	$(CC) -o gpxchess xchess.o $(COBJS) scrollText/libScroll.a \
	$(GPLDFLAGS)

lint:	$(SOURCE)
	lint $(LINTFLAGS) $(SOURCE) $(LINTLIB) | \
	grep -v "multiply declared"

qgrind: $(ALLFILES)
	qgrind -lc $(ALLFILES)

vgrind:	$(ALLFILES)
	vgrind -lc $(ALLFILES)

opt: all

reopt: all

install: all

source:	$(SOURCE) 

tags: $(ALLFILES)
	ctags -w -t *.c *.h > /dev/null 2>&1

wc: $(ALLFILES)
	@wc $(ALLFILES)

print: $(ALLFILES)
	@pr $(ALLFILES)

clean:
	rm -f *.o *.a *.out xchess tags foo tmp

tar:
	tar -cf xchess.tar Makefile *.h *.c *.bitmap *.icon *.cur *.1\
	scrollText/Makefile scrollText/*.h scrollText/*.c scrollText/*.1\
	scrollText/*.3

$(ALLFILES):
	co $@

depend: $(SOURCE)
	cc -M $(CFLAGS) $(CFILES) > makedep
	echo '/^# DO NOT DELETE THIS LINE/+2,$$d' >eddep
	echo '$$r makedep' >>eddep
	echo 'w' >>eddep
	ed - Makefile < eddep
	rm eddep makedep 
	echo '# DEPENDENCIES MUST END AT END OF FILE' >> Makefile
	echo '# IF YOU PUT STUFF HERE IT WILL GO AWAY' >> Makefile
	echo '# see make depend above' >> Makefile

#-----------------------------------------------------------------
# DO NOT DELETE THIS LINE -- make depend uses it
# DEPENDENCIES MUST END AT END OF FILE

xchess.o $(COBJS): $(HFILES)

