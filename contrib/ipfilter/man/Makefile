#
# (C)opyright 1993, 1994, 1995 by Darren Reed.
#
# This code may be freely distributed as long as it retains this notice
# and is not changed in any way.  The author accepts no responsibility
# for the use of this software.  I hate legaleese, don't you ?

all:

install:
	$(INSTALL) -m 0644 -c -o root -g bin ipf.1 $(MANDIR)/man1
	$(INSTALL) -m 0644 -c -o root -g bin ipftest.1 $(MANDIR)/man1
	$(INSTALL) -m 0644 -c -o root -g bin ipnat.1 $(MANDIR)/man1
	$(INSTALL) -m 0644 -c -o root -g bin ipf.4 $(MANDIR)/man4
	$(INSTALL) -m 0644 -c -o root -g bin ipl.4 $(MANDIR)/man4
	$(INSTALL) -m 0644 -c -o root -g bin ipnat.4 $(MANDIR)/man4
	$(INSTALL) -m 0644 -c -o root -g bin ipf.5 $(MANDIR)/man5
	$(INSTALL) -m 0644 -c -o root -g bin ipnat.5 $(MANDIR)/man5
	$(INSTALL) -m 0644 -c -o root -g bin ipmon.8 $(MANDIR)/man8
	$(INSTALL) -m 0644 -c -o root -g bin ipfstat.8 $(MANDIR)/man8
	@echo "Remember to rebuild the whatis database."
