TMACOWN?=	bin
TMACGRP?=	bin
TMACMODE?=	444
TMACDIR?=	/usr/share/tmac

FILES=		0.MT 5.MT 4.MT ms.cov se_ms.cov 
LOCALE=		locale se_locale

MAN7=		groff_mm.7 groff_mmse.7
MLINKS=		groff_mm.7 mm.7 groff_mmse.7 mmse.7

afterinstall:
	install -c -o $(TMACOWN) -g $(TMACGRP) -m $(TMACMODE) \
		$(.CURDIR)/tmac.m $(DESTDIR)$(TMACDIR)/tmac.m
	install -c -o $(TMACOWN) -g $(TMACGRP) -m $(TMACMODE) \
		$(.CURDIR)/tmac.m $(DESTDIR)$(TMACDIR)/tmac.mse

	test -d $(DESTDIR)$(TMACDIR)/mm || mkdir $(DESTDIR)$(TMACDIR)/mm
	chown $(TMACOWN).$(TMACGRP) $(DESTDIR)$(TMACDIR)/mm
	chmod 755 $(DESTDIR)$(TMACDIR)/mm

	for f in $(FILES); do \
		rm -f $(TMACDIR)/mm/$$f; \
		install -c -o $(TMACOWN) -g $(TMACGRP) -m $(TMACMODE) \
			$(.CURDIR)/mm/$$f $(DESTDIR)$(TMACDIR)/mm/$$f; \
	done
	for f in $(LOCALE); do \
		test -f $(DESTDIR)$(TMACDIR)/mm/$$f || touch $(DESTDIR)$(TMACDIR)/mm/$$f; \
		chown $(TMACOWN).$(TMACGRP) $(DESTDIR)$(TMACDIR)/mm/$$f; \
		chmod $(TMACMODE) $(DESTDIR)$(TMACDIR)/mm/$$f; \
	done

.include <bsd.prog.mk>
.include "../Makefile.cfg"
