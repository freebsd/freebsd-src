FILES = ascii eqnchar greek iso utf-8

.c.o:
	$(CC) $(CFLAGS) $(WARN) $(CPPFLAGS) $(EUC) -c $<

all: $(FILES)

utf-8: genutf8
	-./genutf8 >utf-8

genutf8: genutf8.o
	-$(CC) $(CFLAGS) $(LDFLAGS) genutf8.o $(LIBS) -o genutf8

genutf8.o: genutf8.c
	-$(CC) $(CFLAGS) $(WARN) $(CPPFLAGS) $(EUC) -c genutf8.c

install: all
	test -d $(ROOT)$(PUBDIR) || mkdir -p $(ROOT)$(PUBDIR)
	for i in $(FILES); \
	do \
		test -s $$i || continue; \
		$(INSTALL) -c -m 644 $$i $(ROOT)$(PUBDIR)/$$i || exit; \
	done

clean:
	rm -f utf-8 genutf8 genutf8.o core log *~

mrproper: clean
