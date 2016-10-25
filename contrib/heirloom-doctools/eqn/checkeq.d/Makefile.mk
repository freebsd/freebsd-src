VPATH=..
OBJ = checkeq.o

FLAGS =

.c.o:
	$(CC) $(_CFLAGS) $(FLAGS) -c $<

all: checkeq

checkeq: $(OBJ)
	$(CC) $(_CFLAGS) $(_LDFLAGS) $(OBJ) $(LIBS) -o checkeq

install:
	$(INSTALL) -c checkeq $(ROOT)$(BINDIR)/checkeq
	$(STRIP) $(ROOT)$(BINDIR)/checkeq
	rm -f $(ROOT)$(MANDIR)/man1/checkeq.1
	ln -s eqn.1 $(ROOT)$(MANDIR)/man1/checkeq.1

clean:
	rm -f $(OBJ) checkeq core log *~

mrproper: clean
