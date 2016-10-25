OBJ = picpack.o

FLAGS = -I../troff/troff.d/dpost.d

.c.o:
	$(CC) $(_CFLAGS) $(FLAGS) -c $<

all: picpack

picpack: $(OBJ)
	$(CC) $(_CFLAGS) $(_LDFLAGS) $(OBJ) $(LIBS) -o picpack

install:
	$(INSTALL) -c picpack $(ROOT)$(BINDIR)/picpack
	$(STRIP) $(ROOT)$(BINDIR)/picpack
	$(INSTALL) -c -m 644 picpack.1 $(ROOT)$(MANDIR)/man1/picpack.1

clean:
	rm -f $(OBJ) picpack core log *~

mrproper: clean
