#
# $Id: Makefile,v 1.10 1996/11/10 19:50:59 morgan Exp $
#

dummy:

	@echo	"*** This is not a top level Makefile!"

PROGS = blank xsh check_user
SRCS = blank.c xsh.c check_user.c

# have removed the following pair since they no longer conform to
# any recognized conventions: vpass test
# ditto: vpass.c test.c

PROGSUID =

all: $(PROGS)

check_user: check_user.o
	$(CC) $(CFLAGS) -o $@ $< $(LOADLIBES)

blank: blank.o
	$(CC) $(CFLAGS) -o $@ $< $(LOADLIBES)

xsh: xsh.o
	$(CC) $(CFLAGS) -o $@ $< $(LOADLIBES)

install: all
	if [ -n "$(PROGS)" ]; then cp $(PROGS) ../bin ; fi
	if [ -n "$(PROGSUID)" ]; then \
		$(INSTALL) -m 4555 -o root -g bin $(PROGSUID) ../bin ; fi

clean:
	rm -f *.a *.so *.o *~ $(PROGS) $(PROGSUID)

remove:
	cd ../bin ; rm -f $(PROGS) $(PROGSUID)

extraclean: clean
	rm -f *.a *.out *.o *.so
	for x in $(PROGS) $(PROGSUID) ; do rm -f ../bin/$$x ; done
