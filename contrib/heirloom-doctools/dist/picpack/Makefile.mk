OBJ = picpack.o getopt.o

FLAGS = -I../troff/troff.d/dpost.d

.c.o:
	$(CC) $(CFLAGS) $(WARN) $(CPPFLAGS) $(FLAGS) -c $<

all: picpack

picpack: $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJ) $(LIBS) -o picpack

install:
	$(INSTALL) -c picpack $(ROOT)$(BINDIR)/picpack
	$(STRIP) $(ROOT)$(BINDIR)/picpack
	$(INSTALL) -c -m 644 picpack.1 $(ROOT)$(MANDIR)/man1/picpack.1

clean:
	rm -f $(OBJ) picpack core log *~

mrproper: clean
