# Makefile for troff

PROG=		troff
SRCS=		env.cc node.cc input.cc div.cc symbol.cc dictionary.cc reg.cc \
		number.cc majorminor.cc
CFLAGS+=	-I$(.CURDIR)/../include
CXXFLAGS+=	-I$(.CURDIR)/../include
LDADD+=		$(LIBGROFF) -lm
DPADD+=		$(LIBGROFF) $(LIBMATH)
tmacdir?=	/usr/share/tmac

CLEANFILES+=	majorminor.cc

majorminor.cc: $(.CURDIR)/../VERSION
	@echo Making $@
	@-rm -f $@
	@echo const char \*major_version = \
	\"`sed -e 's/^\([^.]*\)\..*$$/\1/' $(.CURDIR)/../VERSION`\"\; >$@
	@echo const char \*minor_version = \
	\"`sed -e 's/^[^.]*\.\([0-9]*\).*$$/\1/' $(.CURDIR)/../VERSION`\"\; >>$@

install_data: hyphen.us
	-test -d $(datadir) || mkdir $(datadir)
	-test -d $(datasubdir) || mkdir $(datasubdir)
	-test -d $(tmacdir) || mkdir $(tmacdir)
	-rm -f $(tmacdir)/hyphen.us
	$(INSTALL_DATA) $(srcdir)/hyphen.us $(tmacdir)/hyphen.us

uninstall_sub:
	-rm -f $(tmacdir)/hyphen.us

afterinstall:
	install -c -o bin -g bin -m 444 $(.CURDIR)/hyphen.us \
		$(DESTDIR)$(tmacdir)/hyphen.us

.include <bsd.prog.mk>
.include "../../../usr.bin/Makefile.inc"
.include "../Makefile.cfg"
