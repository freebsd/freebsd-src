#
# /src/NTP/REPOSITORY/v3/parse/util/Makefile.tmpl,v 3.11 1993/11/17 13:34:12 kardel Exp
#
COMPILER= gcc
DEFS=      -DSYS_FREEBSD -DSYS_386BSD
DEFS_OPT=-DDEBUG
DEFS_LOCAL= $(DEFS_OPT) -DREFCLOCK -DPPSPPS -DKERNEL_PLL
CLOCKDEFS= -DLOCAL_CLOCK -DAS2201PPS -DCHU -DGOES -DIRIG -DMX4200PPS -DOMEGA -DPSTCLK -DTPRO -DWWVBCLK -DMSFEESPPS -DLEITCH
INCL=
COPTS=      -O2
INSTALL= install
BINDIR=	/usr/local/bin
#
CFLAGS=	$(COPTS) $(DEFS) $(DEFS_LOCAL) $(INCL) -I../../include
CC=		$(COMPILER)
TOP=../../
#
EXECS=parsetest testdcf dcfd

all:
	@echo $(DEFS) $(DEFS_LOCAL) $(CLOCKDEFS) | \
	 awk '/-DSTREAM/ && /-DPARSE/ && /-DCLOCK_RAWDCF/ && ( /-DSYS_SUNOS/ || /-DSYS_SOLARIS/ )  { makeit = 1 } \
	      END { if (makeit) \
		      { print "$(MAKE) $(MFLAGS) MFLAGS=\"$(MFLAGS)\" parsetest"; } \
		  }' | \
	 sh
	@echo $(DEFS) $(DEFS_LOCAL) $(CLOCKDEFS) | \
	 awk '/-DPARSE/ && /-DCLOCK_RAWDCF/ && ( /-DSYS_SUNOS/ || /-DSYS_SOLARIS/ ) { makeit = 1 } \
	      END { if (makeit) \
		      { print "$(MAKE) $(MFLAGS) MFLAGS=\"$(MFLAGS)\" testdcf"; } \
		  }' | \
	 sh
	@echo $(DEFS) $(DEFS_LOCAL) $(CLOCKDEFS) | \
	 awk '/-DPARSE/ && /-DCLOCK_RAWDCF/ && ( /-DSYS_SUNOS/  || /-DSYS_SOLARIS/ ) { makeit = 1 } \
	      END { if (makeit) \
		      { print "$(MAKE) $(MFLAGS) MFLAGS=\"$(MFLAGS)\" dcfd"; } \
		  }' | \
	 sh

clean:
	-@rm -f $(EXECS) *.o

distclean:	clean
	-@rm -f *.orig *.rej .version Makefile

install:
	@echo "--- DCF77 utilities should be installed manually"
	@#[ -f testdcf ] && $(INSTALL) -c -m 0755 testdcf $(BINDIR) || true
	@#[ -f dcfd ] && $(INSTALL) -c -m 0755 dcfd $(BINDIR) || true
