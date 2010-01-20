all:
	@echo Nothing to be made.

clean:
	for d in $(SUBDIRS); do make -C $$d clean; done
	rm -f *~ *.o *.so *.d

install:
	if ls *.so >/dev/null 2>&1; then \
		install -d $(DESTDIR)$(LIBDIR)/wpa_supplicant && \
		cp *.so $(DESTDIR)$(LIBDIR)/wpa_supplicant \
	; fi
