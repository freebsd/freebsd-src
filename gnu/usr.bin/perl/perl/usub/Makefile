SRC = ..
GLOBINCS = 
LOCINCS = 
LIBS = -lcurses -ltermlib `. $(SRC)/config.sh; echo $$libs`

curseperl: $(SRC)/uperl.o usersub.o curses.o
	cc $(SRC)/uperl.o usersub.o curses.o $(LIBS) -o curseperl

usersub.o: usersub.c
	cc -c -I$(SRC) $(GLOBINCS) -DDEBUGGING -g usersub.c

curses.o: curses.c
	cc -c -I$(SRC) $(GLOBINCS) -DDEBUGGING -g curses.c

curses.c: curses.mus
	mus curses.mus >curses.c
