#
# Redistribution and use in source and binary forms are permitted
# provided that this notice is preserved and due credit is given
# to the original author and the contributors.
#
#CC=gcc -Wuninitialized -Wstrict-prototypes -Werror -O
CFLAGS=-I..

all: $(DESTDIR)/y.tab.o $(DESTDIR)/lex.yy.o

$(DESTDIR)/y.tab.o: $(DESTDIR)/y.tab.c
	$(CC) $(DEBUG) -I. -I.. -I$(DESTDIR) -I../ipsend $(CFLAGS) $(LINUX) -c $(DESTDIR)/y.tab.c -o $@

$(DESTDIR)/$(OBJ)/y.tab.o: $(DESTDIR)/y.tab.c
	$(CC) $(DEBUG) -I. -I.. -I$(DESTDIR) -I../ipsend $(CFLAGS) $(LINUX) -c $(DESTDIR)/y.tab.c -o $@

$(DESTDIR)/lex.yy.o: $(DESTDIR)/lex.yy.c
	$(CC) $(DEBUG) -I. -I.. -I$(DESTDIR) -I../ipsend $(CFLAGS) $(LINUX) -c $(DESTDIR)/lex.yy.c -o $@

y.tab.o: y.tab.c
	$(CC) $(DEBUG) -I. -I.. -I../ipsend $(CFLAGS) $(LINUX) -c y.tab.c -o $@

lex.yy.o: lex.yy.c
	$(CC) $(DEBUG) -I. -I.. -I../ipsend $(CFLAGS) $(LINUX) -c lex.yy.c -o $@

$(DESTDIR)/lex.yy.c: iplang_l.l $(DESTDIR)/y.tab.h
	lex iplang_l.l
	mv lex.yy.c $(DESTDIR)

$(DESTDIR)/y.tab.c $(DESTDIR)/y.tab.h: iplang_y.y
	yacc -d iplang_y.y
	mv y.tab.c $(DESTDIR)
	mv y.tab.h $(DESTDIR)

clean:
	/bin/rm -f *.o lex.yy.c y.tab.c y.tab.h
