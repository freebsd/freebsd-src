#
# Copyright (C) 1993-1998 by Darren Reed.
#
# Redistribution and use in source and binary forms are permitted
# provided that this notice is preserved and due credit is given
# to the original author and the contributors.
#

all:

install:
	$(INSTALL) -m 0644 -c -o root -g bin ipftest.1 $(MANDIR)/man1
	$(INSTALL) -m 0644 -c -o root -g bin ipnat.1 $(MANDIR)/man1
	$(INSTALL) -m 0644 -c -o root -g bin ipf.4 $(MANDIR)/man4
	$(INSTALL) -m 0644 -c -o root -g bin ipl.4 $(MANDIR)/man4
	$(INSTALL) -m 0644 -c -o root -g bin ipnat.4 $(MANDIR)/man4
	$(INSTALL) -m 0644 -c -o root -g bin ipf.5 $(MANDIR)/man5
	$(INSTALL) -m 0644 -c -o root -g bin ipnat.5 $(MANDIR)/man5
	$(INSTALL) -m 0644 -c -o root -g bin ipf.8 $(MANDIR)/man8
	$(INSTALL) -m 0644 -c -o root -g bin ipfs.8 $(MANDIR)/man8
	$(INSTALL) -m 0644 -c -o root -g bin ipmon.8 $(MANDIR)/man8
	$(INSTALL) -m 0644 -c -o root -g bin ipfstat.8 $(MANDIR)/man8
	@echo "Remember to rebuild the whatis database."
