#
# $Id: Makefile,v 1.4 2001/02/10 07:17:53 agmorgan Exp $
#

include ../Make.Rules

PROGS = blank xsh check_user
SRCS = blank.c xsh.c check_user.c
PROGSUID =

ifeq ($(WITH_LIBDEBUG),yes)
 LIBSUFFIX=d
else
 LIBSUFFIX=
endif

CFLAGS += -I../libpam_misc/include -I../libpamc/include

LOADLIBES = -L../libpam -L../libpamc -L../libpam_misc \
		-lpam$(LIBSUFFIX) -lpam_misc$(LIBSUFFIX)

ifeq ($(STATIC_LIBPAM),yes)
  ifneq ($(DYNAMIC),)
    CFLAGS += $(CC_STATIC)
    LOADLIBES += $(LIBDL)
  endif
endif

all: $(PROGS)

check_user: check_user.o
	$(CC) $(CFLAGS) -o $@ $< $(LOADLIBES)

blank: blank.o
	$(CC) $(CFLAGS) -o $@ $< $(LOADLIBES)

xsh: xsh.o
	$(CC) $(CFLAGS) -o $@ $< $(LOADLIBES)

clean:
	rm -f *.a *.so *.o *~ $(PROGS) $(PROGSUID)
	rm -f *.a *.out *.o *.so

# note, the programs are test programs, they should not be
# installed on your system!

install: all
	if [ -n "$(PROGS)" ]; then cp $(PROGS) ../bin ; fi
	if [ -n "$(PROGSUID)" ]; then \
		$(INSTALL) -m 4555 $(PROGSUID) ../bin ; fi

remove:
	cd ../bin ; rm -f $(PROGS) $(PROGSUID)
	for x in $(PROGS) $(PROGSUID) ; do rm -f ../bin/$$x ; done
