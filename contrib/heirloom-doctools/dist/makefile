SUBDIRS = \
	stuff/bst \
	eqn/eqn.d \
	eqn/neqn.d \
	eqn/checkeq.d \
	eqn/eqnchar.d \
	tbl \
	pic \
	grap \
	vgrind \
	refer \
	mpm \
	troff/libhnj \
	troff/libhnj/hyphen.d \
	troff/troff.d/font \
	troff/troff.d/font/devhtml \
	troff/troff.d/tmac.d \
	troff/troff.d/postscript \
	troff/troff.d \
	troff/troff.d/dpost.d \
	troff/troff.d/dhtml \
	troff/nroff.d \
	troff/nroff.d/terms.d \
	picpack \
	checknr \
	ptx \
	soelim \
	col

# Removed from SUBDIRS. Add again if required.
#	troff/troff.d/devaps

MAKEFILES = $(SUBDIRS:=/Makefile)

TESTDIRS = \
	doc/fonts \
	doc/just \
	doc/quickstart \
	doc/troff \
	test

.PHONY:	test
.SUFFIXES: .mk
.mk:
	cat cfg.mk $< >$@

dummy: cfg.mk $(MAKEFILES) all

makefiles: $(MAKEFILES)

.DEFAULT:
	+ for i in $(SUBDIRS); \
	do \
		(cd "$$i" && $(MAKE) $@) || exit; \
	done

mrproper: clean
	rm -f cfg.mk config.log compat.h
	+ for i in $(SUBDIRS); \
	do \
		(cd "$$i" && $(MAKE) $@) || exit; \
	done
	rm -f $(MAKEFILES)

test:
	for i in $(TESTDIRS); do \
		(cd $$i && $(MAKE) $@) || exit 1; \
	done
	@printf "\n*** TEST FINISHED SUCCESSFUL ***\n"

PKGROOT = /var/tmp/heirloom-devtools
PKGTEMP = /var/tmp
PKGPROTO = pkgproto

heirloom-doctools.pkg: all
	rm -rf $(PKGROOT)
	mkdir -p $(PKGROOT)
	$(MAKE) ROOT=$(PKGROOT) install
	rm -f $(PKGPROTO)
	echo 'i pkginfo' >$(PKGPROTO)
	(cd $(PKGROOT) && find . -print | pkgproto) | >>$(PKGPROTO) sed 's:^\([df] [^ ]* [^ ]* [^ ]*\) .*:\1 root root:; s:^f\( [^ ]* etc/\):v \1:; s:^f\( [^ ]* var/\):v \1:; s:^\(s [^ ]* [^ ]*=\)\([^/]\):\1./\2:'
	rm -rf $(PKGTEMP)/$@
	pkgmk -a `uname -m` -d $(PKGTEMP) -r $(PKGROOT) -f $(PKGPROTO) $@
	pkgtrans -o -s $(PKGTEMP) `pwd`/$@ $@
	rm -rf $(PKGROOT) $(PKGPROTO) $(PKGTEMP)/$@

cfg.mk:
	./configure
