## Process this file with automake to generate Makefile.in

AUTOMAKE_OPTIONS = cygnus

# What version of the manual you want; "all" includes everything
CONFIG=all

man_MANS = as.1

info_TEXINFOS = as.texinfo gasp.texi

asconfig.texi: $(CONFIG).texi
	rm -f asconfig.texi
	ln -s $(srcdir)/$(CONFIG).texi ./asconfig.texi >/dev/null 2>&1 \
	  || ln $(srcdir)/$(CONFIG).texi ./asconfig.texi >/dev/null 2>&1 \
	  || cp $(srcdir)/$(CONFIG).texi ./asconfig.texi

CPU_DOCS = \
	c-a29k.texi \
	c-arm.texi \
	c-d10v.texi \
	c-h8300.texi \
	c-h8500.texi \
	c-hppa.texi \
	c-i370.texi \
	c-i386.texi \
	c-i960.texi \
	c-m32r.texi \
	c-m68k.texi \
	c-mips.texi \
	c-ns32k.texi \
	c-pj.texi \
	c-sh.texi \
	c-sparc.texi \
	c-vax.texi \
	c-v850.texi \
	c-z8k.texi

gasver.texi: Makefile
	rm -f $@
	echo '@set VERSION $(VERSION)' > $@

as.info: $(srcdir)/as.texinfo asconfig.texi gasver.texi $(CPU_DOCS)
as.dvi: $(srcdir)/as.texinfo asconfig.texi gasver.texi $(CPU_DOCS)

# This one isn't ready for prime time yet.  Not even a little bit.

noinst_TEXINFOS = internals.texi

DISTCLEANFILES = asconfig.texi

MAINTAINERCLEANFILES = gasver.texi
