
install:
	install -c -o $(BINOWN) -g $(BINGRP) -m $(BINMODE) \
		$(.CURDIR)/cpp.script $(DESTDIR)/usr/bin/cpp
	@/bin/rm -rf $(DESTDIR)/usr/bin/gcpp
	@cd $(DESTDIR)/usr/bin; ln -s cpp gcpp
	@echo $(DESTDIR)/usr/bin/gcpp -\> $(DESTDIR)/usr/bin/cpp
	

.include <bsd.prog.mk>
