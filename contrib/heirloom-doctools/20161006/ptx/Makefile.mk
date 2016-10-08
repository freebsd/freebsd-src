OBJ = ptx.o

FLAGS = -DLIBDIR='"$(LIBDIR)"' $(EUC)

.c.o:
	$(CC) $(CFLAGS) $(WARN) $(CPPFLAGS) $(FLAGS) -c $<

all: ptx

ptx: $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJ) $(LIBS) -o ptx

install:
	$(INSTALL) -c ptx $(ROOT)$(BINDIR)/ptx
	$(STRIP) $(ROOT)$(BINDIR)/ptx
	$(INSTALL) -c -m 644 ptx.1 $(ROOT)$(MANDIR)/man1/ptx.1
	test -d $(ROOT)$(LIBDIR) || mkdir -p $(ROOT)$(LIBDIR)
	$(INSTALL) -c -m 644 eign $(ROOT)$(LIBDIR)/eign

clean:
	rm -f $(OBJ) ptx core log *~

mrproper: clean
