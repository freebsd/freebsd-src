## Process this file with automake to produce Makefile.in
SUBDIRS = lib bc dc doc

dist-hook:
	mkdir $(distdir)/h $(distdir)/Examples $(distdir)/Test
	cp -p $(srcdir)/h/*.h $(distdir)/h
	cp -p $(srcdir)/Examples/*.b $(distdir)/Examples
	cp -p $(srcdir)/Test/*.b $(srcdir)/Test/*.bc $(distdir)/Test
	cp -p $(srcdir)/Test/signum $(srcdir)/Test/timetest $(distdir)/Test
