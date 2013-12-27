SUBDIRS = src

EXTRA_DIST = libsdp.spec.in libsdp.conf

dist-hook: libsdp.spec
	cp libsdp.spec $(distdir)

install-data-hook:
	if test -e $(DESTDIR)$(sysconfdir)/libsdp.conf; then \
		diff -q $(srcdir)/libsdp.conf $(DESTDIR)$(sysconfdir)/libsdp.conf 1> /dev/null; \
		if test $$? == 1; then \
			t=$(shell date +'%Y%m%d%H%M%S'); \
		   cp -p $(srcdir)/libsdp.conf \
				$(DESTDIR)$(sysconfdir)/libsdp.conf.$$t; \
			echo "NOTE: existing libsdp.conf was not updated."; \
			echo "      libsdp.conf installed as ibsdp.conf.$$t instead."; \
		fi; \
	else \
		if test ! -d $(DESTDIR)$(sysconfdir); then \
			mkdir -p $(DESTDIR)$(sysconfdir); \
		fi; \
		cp -p $(srcdir)/libsdp.conf $(DESTDIR)$(sysconfdir)/libsdp.conf; \
	fi
