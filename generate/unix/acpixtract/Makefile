#
# acpixtract utility
#
include ../Makefile.config

PROG = acpixtract


$(PROG) : $(ACPICA_TOOLS)/acpixtract/acpixtract.c
	$(CC) $(CFLAGS) $(CWARNINGFLAGS) $(LDFLAGS) -o$(PROG) $?
	$(COPYPROG)

clean : 
	rm -f $(PROG) $(PROG).exe

install : 
	$(INSTALLPROG)
