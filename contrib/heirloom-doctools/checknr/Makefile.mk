OBJ = checknr.o

FLAGS = $(DEFINES) -I../include

.c.o:
	$(CC) $(_CFLAGS) $(FLAGS) -c $<

all: checknr

checknr: $(OBJ)
	$(CC) $(_CFLAGS) $(_LDFLAGS) $(OBJ) $(LIBS) -o checknr

install:
	$(INSTALL) -c checknr $(ROOT)$(BINDIR)/checknr
	$(STRIP) $(ROOT)$(BINDIR)/checknr
	$(INSTALL) -c -m 644 checknr.1 $(ROOT)$(MANDIR)/man1/checknr.1

clean:
	rm -f $(OBJ) checknr core log *~

mrproper: clean
