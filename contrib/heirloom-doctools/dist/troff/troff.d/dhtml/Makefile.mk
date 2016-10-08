BST=		../../../stuff/bst
BIN=		dhtml
OBJ=		main.o dhtml.o tr_out.o char.o lib.o $(BST)/bst.o
CPPFLAGS=	-DFNTDIR='"$(FNTDIR)"' -I$(BST)

all:		$(BIN)

install:
		$(STRIP) $(BIN)
		$(INSTALL) $(BIN) $(ROOT)$(BINDIR)/
		sed 's"$$FNTDIR"$(FNTDIR)"g' $(BIN).1 > \
		    $(ROOT)$(MANDIR)/man1/$(BIN).1

clean:
		rm -f $(OBJ) $(BIN)

mrproper:	clean

$(BIN):		$(OBJ)
		$(CC) $(CFLAGS) $(LDFLAGS) $(OBJ) -o $@

.c.o:
		$(CC) $(CFLAGS) $(CPPFLAGS) -c $<

char.o:		char.h main.h $(BST)/bst.h lib.h tr_out.h
dhtml.o:	dhtml.h tr_out.h char.h main.h
lib.o:		main.h $(BST)/bst.h
main.o:		dhtml.h char.h
tr_out.o:	tr_out.h main.h $(BST)/bst.h lib.h
