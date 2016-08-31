## Process this file with automake to produce Makefile.in

# Package: am-utils
# Level: Makefile for doc/ directory
# Author: Erez Zadok

@SET_MAKE@

info_TEXINFOS = am-utils.texi

EXTRA_DIST = hlfsd.ps

# force generation of 'letter' format PS and embed Type-3 fonts
# (our European allies may object...)
DVIPS = dvips -t letter -Pcmz

# Force version.texi to be rebuilt regardless of maintainer mode
$(srcdir)/version.texi: am-utils.texi $(top_srcdir)/configure.ac
	@(set `$(SHELL) $(srcdir)/mdate-sh $(srcdir)/am-utils.texi`; \
	echo "@set UPDATED $$1 $$2 $$3"; \
	echo "@set UPDATED-MONTH $$2 $$3"; \
	echo "@set EDITION $(VERSION)"; \
	echo "@set VERSION $(VERSION)") > vti.tmp
	@cmp -s vti.tmp $(srcdir)/version.texi \
	  || (echo "Updating $(srcdir)/version.texi"; \
	      cp vti.tmp $(srcdir)/version.texi)
	-@rm -f vti.tmp

# include dvi/ps in distribution
dist-hook: $(distdir)/am-utils.dvi $(distdir)/am-utils.ps

$(distdir)/am-utils.dvi: am-utils.dvi
	install -c -m 644 $< $@

$(distdir)/am-utils.ps: am-utils.ps
	install -c -m 644 $< $@

# a special target for making Postscript files
ps: am-utils.dvi am-utils.ps

am-utils-clean: clean maintainer-clean-aminfo maintainer-clean-vti

# personal rule for Erez Zadok to generate and install html docs
alldocs: ps install-NEWS install-INSTALL install-ChangeLog install-AUTHORS install-tasks install-autofs install-y2k install-ldap install-osx install-BUGS install-FAQ install-html install-ps install-MIRRORS install-attrcache install-sun2amd

AMU_HTML_TOP=$(HOME)/proj/www/docs/am-utils
install-html: $(AMU_HTML_TOP)/am-utils-all.html
$(AMU_HTML_TOP)/am-utils-all.html: am-utils.texi
	texi2html -Verbose -expand info -number -out_file am-utils-all.html $<
	texi2html -frames -Verbose -expand info -number -split chapter $<
	mv am-utils-all.html am-utils/*.html $(AMU_HTML_TOP)
	chmod -R a+r $(AMU_HTML_TOP)

install-ps: $(AMU_HTML_TOP)/am-utils.ps
$(AMU_HTML_TOP)/am-utils.ps: am-utils.ps
	install -c -m 644 $< $@

install-NEWS: $(AMU_HTML_TOP)/NEWS.txt
$(AMU_HTML_TOP)/NEWS.txt: $(top_srcdir)/NEWS
	install -c -m 644 $< $@

install-INSTALL: $(AMU_HTML_TOP)/INSTALL.txt
$(AMU_HTML_TOP)/INSTALL.txt: $(top_srcdir)/INSTALL
	install -c -m 644 $< $@

install-ChangeLog: $(AMU_HTML_TOP)/ChangeLog.txt
$(AMU_HTML_TOP)/ChangeLog.txt: $(top_srcdir)/ChangeLog
	sed 's/@.*>$$/@...>/g' < $< > $@
	chmod 644 $@

install-AUTHORS: $(AMU_HTML_TOP)/AUTHORS.txt
$(AMU_HTML_TOP)/AUTHORS.txt: $(top_srcdir)/AUTHORS
	install -c -m 644 $< $@

install-tasks: $(AMU_HTML_TOP)/tasks.txt
$(AMU_HTML_TOP)/tasks.txt: $(top_srcdir)/tasks
	install -c -m 644 $< $@

install-autofs: $(AMU_HTML_TOP)/autofs.txt
$(AMU_HTML_TOP)/autofs.txt: $(top_srcdir)/README.autofs
	install -c -m 644 $< $@

install-y2k: $(AMU_HTML_TOP)/y2k.txt
$(AMU_HTML_TOP)/y2k.txt: $(top_srcdir)/README.y2k
	install -c -m 644 $< $@

install-attrcache: $(AMU_HTML_TOP)/attrcache.txt
$(AMU_HTML_TOP)/attrcache.txt: $(top_srcdir)/README.attrcache
	install -c -m 644 $< $@

install-sun2amd: $(AMU_HTML_TOP)/sun2amd.txt
$(AMU_HTML_TOP)/sun2amd.txt: $(top_srcdir)/README.sun2amd
	install -c -m 644 $< $@

install-ldap: $(AMU_HTML_TOP)/ldap.txt
$(AMU_HTML_TOP)/ldap.txt: $(top_srcdir)/README.ldap
	install -c -m 644 $< $@

install-osx: $(AMU_HTML_TOP)/osx.txt
$(AMU_HTML_TOP)/osx.txt: $(top_srcdir)/README.osx
	install -c -m 644 $< $@

install-BUGS: $(AMU_HTML_TOP)/BUGS.txt
$(AMU_HTML_TOP)/BUGS.txt: $(top_srcdir)/BUGS
	install -c -m 644 $< $@

install-FAQ: $(AMU_HTML_TOP)/FAQ.txt
$(AMU_HTML_TOP)/FAQ.txt: $(top_srcdir)/FAQ
	install -c -m 644 $< $@

install-MIRRORS: $(AMU_HTML_TOP)/MIRRORS.txt $(AMU_HTML_TOP)/MIRRORS.html
$(AMU_HTML_TOP)/MIRRORS.txt: $(top_srcdir)/MIRRORS.txt
	install -c -m 644 $< $@
$(AMU_HTML_TOP)/MIRRORS.html: $(top_srcdir)/MIRRORS.html
	install -c -m 644 $< $@
