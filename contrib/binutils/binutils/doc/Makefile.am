## Process this file with automake to generate Makefile.in

AUTOMAKE_OPTIONS = cygnus

# What version of the manual you want; "all" includes everything
CONFIG=all

# Options to extract the man page from as.texinfo
MANCONF = -Dman

TEXI2POD = perl $(top_srcdir)/../etc/texi2pod.pl

POD2MAN = pod2man --center="GNU Development Tools" \
	 --release="binutils-$(VERSION)" --section=1

# List of man pages generated from binutils.texi
man_MANS = \
	addr2line.1 \
	ar.1 \
	dlltool.1 \
	nlmconv.1 \
	nm.1 \
	objcopy.1 \
	objdump.1 \
	ranlib.1 \
	readelf.1 \
	size.1 \
	strings.1 \
	strip.1 \
	windres.1 \
	$(DEMANGLER_NAME).1

info_TEXINFOS = binutils.texi

config.texi: Makefile
	rm -f config.texi
	echo '@set VERSION $(VERSION)' > config.texi

binutils_TEXI = $(srcdir)/binutils.texi

binutils.dvi:  $(binutils_TEXI) config.texi

binutils.info: $(binutils_TEXI) config.texi

# Man page generation from texinfo
addr2line.1:	$(binutils_TEXI)
	touch $@
	-$(TEXI2POD) $(MANCONF) -Daddr2line < $(binutils_TEXI) > addr2line.pod
	-($(POD2MAN) addr2line.pod | sed -e '/^.if n .na/d' > $@.T$$$$ && \
		mv -f $@.T$$$$ $@) || (rm -f $@.T$$$$ && exit 1)
	rm -f addr2line.pod

ar.1:	$(binutils_TEXI)
	touch $@
	-$(TEXI2POD) $(MANCONF) -Dar < $(binutils_TEXI) > ar.pod
	-($(POD2MAN) ar.pod | sed -e '/^.if n .na/d' > $@.T$$$$ && \
		mv -f $@.T$$$$ $@) || (rm -f $@.T$$$$ && exit 1)
	rm -f ar.pod

dlltool.1:	$(binutils_TEXI)
	touch $@
	-$(TEXI2POD) $(MANCONF) -Ddlltool < $(binutils_TEXI) > dlltool.pod
	-($(POD2MAN) dlltool.pod | sed -e '/^.if n .na/d' > $@.T$$$$ && \
		mv -f $@.T$$$$ $@) || (rm -f $@.T$$$$ && exit 1)
	rm -f dlltool.pod

nlmconv.1:	$(binutils_TEXI)
	touch $@
	-$(TEXI2POD) $(MANCONF) -Dnlmconv < $(binutils_TEXI) > nlmconv.pod
	-($(POD2MAN) nlmconv.pod | sed -e '/^.if n .na/d' > $@.T$$$$ && \
		mv -f $@.T$$$$ $@) || (rm -f $@.T$$$$ && exit 1)
	rm -f nlmconv.pod

nm.1:	$(binutils_TEXI)
	touch $@
	-$(TEXI2POD) $(MANCONF) -Dnm < $(binutils_TEXI) > nm.pod
	-($(POD2MAN) nm.pod | sed -e '/^.if n .na/d' > $@.T$$$$ && \
		mv -f $@.T$$$$ $@) || (rm -f $@.T$$$$ && exit 1)
	rm -f nm.pod

objcopy.1:	$(binutils_TEXI)
	touch $@
	-$(TEXI2POD) $(MANCONF) -Dobjcopy < $(binutils_TEXI) > objcopy.pod
	-($(POD2MAN) objcopy.pod | sed -e '/^.if n .na/d' > $@.T$$$$ && \
		mv -f $@.T$$$$ $@) || (rm -f $@.T$$$$ && exit 1)
	rm -f objcopy.pod

objdump.1:	$(binutils_TEXI)
	touch $@
	-$(TEXI2POD) $(MANCONF) -Dobjdump < $(binutils_TEXI) > objdump.pod
	-($(POD2MAN) objdump.pod | sed -e '/^.if n .na/d' > $@.T$$$$ && \
		mv -f $@.T$$$$ $@) || (rm -f $@.T$$$$ && exit 1)
	rm -f objdump.pod

ranlib.1:	$(binutils_TEXI)
	touch $@
	-$(TEXI2POD) $(MANCONF) -Dranlib < $(binutils_TEXI) > ranlib.pod
	-($(POD2MAN) ranlib.pod | sed -e '/^.if n .na/d' > $@.T$$$$ && \
		mv -f $@.T$$$$ $@) || (rm -f $@.T$$$$ && exit 1)
	rm -f ranlib.pod

readelf.1:	$(binutils_TEXI)
	touch $@
	-$(TEXI2POD) $(MANCONF) -Dreadelf < $(binutils_TEXI) > readelf.pod
	-($(POD2MAN) readelf.pod | sed -e '/^.if n .na/d' > $@.T$$$$ && \
		mv -f $@.T$$$$ $@) || (rm -f $@.T$$$$ && exit 1)
	rm -f readelf.pod

size.1:	$(binutils_TEXI)
	touch $@
	-$(TEXI2POD) $(MANCONF) -Dsize < $(binutils_TEXI) > size.pod
	-($(POD2MAN) size.pod | sed -e '/^.if n .na/d' > $@.T$$$$ && \
		mv -f $@.T$$$$ $@) || (rm -f $@.T$$$$ && exit 1)
	rm -f size.pod

strings.1:	$(binutils_TEXI)
	touch $@
	-$(TEXI2POD) $(MANCONF) -Dstrings < $(binutils_TEXI) > strings.pod
	-($(POD2MAN) strings.pod | sed -e '/^.if n .na/d' > $@.T$$$$ && \
		mv -f $@.T$$$$ $@) || (rm -f $@.T$$$$ && exit 1)
	rm -f strings.pod

strip.1:	$(binutils_TEXI)
	touch $@
	-$(TEXI2POD) $(MANCONF) -Dstrip < $(binutils_TEXI) > strip.pod
	-($(POD2MAN) strip.pod | sed -e '/^.if n .na/d' > $@.T$$$$ && \
		mv -f $@.T$$$$ $@) || (rm -f $@.T$$$$ && exit 1)
	rm -f strip.pod

windres.1:	$(binutils_TEXI)
	touch $@
	-$(TEXI2POD) $(MANCONF) -Dwindres < $(binutils_TEXI) > windres.pod
	-($(POD2MAN) windres.pod | sed -e '/^.if n .na/d' > $@.T$$$$ && \
		mv -f $@.T$$$$ $@) || (rm -f $@.T$$$$ && exit 1)
	rm -f windres.pod

cxxfilt.man:	$(binutils_TEXI)
	touch $@
	-$(TEXI2POD) $(MANCONF) -Dcxxfilt < $(binutils_TEXI) > $(DEMANGLER_NAME).pod
	-($(POD2MAN) $(DEMANGLER_NAME).pod | sed -e '/^.if n .na/d' > $@.T$$$$ && \
		mv -f $@.T$$$$ $@) || (rm -f $@.T$$$$ && exit 1)
	rm -f $(DEMANGLER_NAME).pod

MAINTAINERCLEANFILES = config.texi
DISTCLEANFILES       = config.texi
MOSTLYCLEANFILES     = $(DEMANGLER_NAME).1

$(DEMANGLER_NAME).1: cxxfilt.man Makefile
	if test -f cxxfilt.man; then \
	  man=cxxfilt.man; \
	else \
	  man=$(srcdir)/cxxfilt.man; \
	fi; \
	sed -e 's/@PROGRAM@/$(DEMANGLER_NAME)/' \
	    -e 's/cxxfilt/$(DEMANGLER_NAME)/' < $$man \
		> $(DEMANGLER_NAME).1

# We want install to imply install-info as per GNU standards, despite the
# cygnus option.
install: install-info

# Maintenance

# We need it for the taz target in ../../Makefile.in.
info: $(MANS)
