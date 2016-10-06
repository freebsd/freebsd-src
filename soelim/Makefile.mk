OBJ = soelim.o

FLAGS =

.c.o:
	$(CC) $(CFLAGS) $(WARN) $(CPPFLAGS) $(FLAGS) -c $<

all: soelim

soelim: $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJ) $(LIBS) -o soelim

install:
	$(INSTALL) -c soelim $(ROOT)$(BINDIR)/soelim
	$(STRIP) $(ROOT)$(BINDIR)/soelim
	$(INSTALL) -c -m 644 soelim.1 $(ROOT)$(MANDIR)/man1/soelim.1

clean:
	rm -f $(OBJ) soelim core log *~

mrproper: clean
