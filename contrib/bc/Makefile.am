## Process this file with automake to produce Makefile.in

SUBDIRS = lib bc dc doc

MAINTAINERCLEANFILES =  aclocal.m4 config.h.in configure Makefile.in \
			$(distdir).tar.gz h/number.h

dist-hook:
	mkdir $(distdir)/h $(distdir)/Examples $(distdir)/Test
	cp -p $(srcdir)/h/*.h $(distdir)/h
	cp -p $(srcdir)/Examples/*.b $(distdir)/Examples
	cp -p $(srcdir)/Test/*.b $(srcdir)/Test/*.bc $(distdir)/Test
	cp -p $(srcdir)/Test/signum $(srcdir)/Test/timetest $(distdir)/Test
	cp -p $(srcdir)/lib/testmul.c $(distdir)/lib
	cp -p $(srcdir)/FAQ $(distdir)

timetest:
	(cd lib; $(MAKE) specialnumber)
