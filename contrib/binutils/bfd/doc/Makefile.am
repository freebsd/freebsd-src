## Process this file with automake to generate Makefile.in

AUTOMAKE_OPTIONS = cygnus

DOCFILES = aoutx.texi  archive.texi archures.texi \
	bfdt.texi  cache.texi coffcode.texi \
	core.texi elf.texi elfcode.texi  format.texi  libbfd.texi  \
	opncls.texi  reloc.texi  section.texi  \
	syms.texi  targets.texi init.texi hash.texi linker.texi \
	mmo.texi

PROTOS = archive.p archures.p bfd.p \
	 core.p format.p \
	libbfd.p opncls.p reloc.p \
	section.p syms.p targets.p  \
	format.p  core.p init.p

IPROTOS = cache.ip libbfd.ip reloc.ip init.ip archures.ip coffcode.ip

# SRCDOC, SRCPROT, SRCIPROT only used to sidestep Sun Make bug in interaction 
# between VPATH and suffix rules.  If you use GNU Make, perhaps other Makes,
# you don't need these three:
SRCDOC = $(srcdir)/../aoutx.h  $(srcdir)/../archive.c \
	$(srcdir)/../archures.c $(srcdir)/../bfd.c \
	$(srcdir)/../cache.c $(srcdir)/../coffcode.h \
	$(srcdir)/../corefile.c $(srcdir)/../elf.c \
	$(srcdir)/../elfcode.h  $(srcdir)/../format.c \
	$(srcdir)/../libbfd.c $(srcdir)/../opncls.c \
	$(srcdir)/../reloc.c  $(srcdir)/../section.c \
	$(srcdir)/../syms.c  $(srcdir)/../targets.c \
	$(srcdir)/../hash.c $(srcdir)/../linker.c \
	$(srcdir)/../mmo.c

SRCPROT = $(srcdir)/../archive.c $(srcdir)/../archures.c \
	$(srcdir)/../bfd.c $(srcdir)/../coffcode.h $(srcdir)/../corefile.c \
	$(srcdir)/../format.c $(srcdir)/../libbfd.c \
	$(srcdir)/../opncls.c $(srcdir)/../reloc.c \
	$(srcdir)/../section.c $(srcdir)/../syms.c \
	$(srcdir)/../targets.c $(srcdir)/../init.c

SRCIPROT = $(srcdir)/../cache.c $(srcdir)/../libbfd.c \
	$(srcdir)/../reloc.c $(srcdir)/../cpu-h8300.c \
	$(srcdir)/../cpu-i960.c $(srcdir)/../archures.c \
	$(srcdir)/../init.c

TEXIDIR = $(srcdir)/../../texinfo/fsf

info_TEXINFOS = bfd.texinfo

MKDOC = chew$(EXEEXT_FOR_BUILD)

$(MKDOC): chew.o
	$(CC_FOR_BUILD) -o $(MKDOC) chew.o $(LOADLIBES) $(LDFLAGS)

chew.o: chew.c
	$(CC_FOR_BUILD) -c -I.. -I$(srcdir)/.. -I$(srcdir)/../../include -I$(srcdir)/../../intl -I../../intl $(H_CFLAGS) $(CFLAGS) $(srcdir)/chew.c

protos: libbfd.h libcoff.h bfd.h

bfd.info bfd.dvi: $(DOCFILES) bfdsumm.texi bfd.texinfo

# We can't replace these rules with an implicit rule, because
# makes without VPATH support couldn't find the .h files in `..'.

# We use s-XXX targets so that we can distribute the info files,
# and permit people to rebuild them, without requiring the makeinfo
# program.  If somebody tries to rebuild info, but none of the .texi
# files have changed, then this Makefile will build chew, and will
# build all of the stamp files, but will not actually have to rebuild
# bfd.info.

s-aoutx: $(MKDOC) $(srcdir)/../aoutx.h $(srcdir)/doc.str
	./$(MKDOC) -f $(srcdir)/doc.str <$(srcdir)/../aoutx.h >aoutx.tmp
	$(srcdir)/../../move-if-change aoutx.tmp aoutx.texi
	touch s-aoutx
aoutx.texi: s-aoutx

s-archive: $(MKDOC) $(srcdir)/../archive.c $(srcdir)/doc.str
	./$(MKDOC) -f $(srcdir)/doc.str <$(srcdir)/../archive.c >archive.tmp
	$(srcdir)/../../move-if-change archive.tmp archive.texi
	touch s-archive
archive.texi: s-archive

s-archures: $(MKDOC) $(srcdir)/../archures.c $(srcdir)/doc.str
	./$(MKDOC) -f $(srcdir)/doc.str < $(srcdir)/../archures.c >archures.tmp
	$(srcdir)/../../move-if-change archures.tmp archures.texi
	touch s-archures
archures.texi: s-archures

# We use bfdt.texi, rather than bfd.texi, to avoid conflicting with
# bfd.texinfo on an 8.3 filesystem.
s-bfd: $(MKDOC) $(srcdir)/../bfd.c $(srcdir)/doc.str
	./$(MKDOC) -f $(srcdir)/doc.str < $(srcdir)/../bfd.c >bfd.tmp
	$(srcdir)/../../move-if-change bfd.tmp bfdt.texi
	touch s-bfd
bfdt.texi: s-bfd

s-cache: $(MKDOC) $(srcdir)/../cache.c $(srcdir)/doc.str
	./$(MKDOC) -f $(srcdir)/doc.str < $(srcdir)/../cache.c >cache.tmp
	$(srcdir)/../../move-if-change cache.tmp cache.texi
	touch s-cache
cache.texi: s-cache

s-coffcode: $(MKDOC) $(srcdir)/../coffcode.h $(srcdir)/doc.str
	./$(MKDOC) -f $(srcdir)/doc.str <$(srcdir)/../coffcode.h >coffcode.tmp
	$(srcdir)/../../move-if-change coffcode.tmp coffcode.texi
	touch s-coffcode
coffcode.texi: s-coffcode

s-core: $(MKDOC) $(srcdir)/../corefile.c $(srcdir)/doc.str
	./$(MKDOC) -f $(srcdir)/doc.str <$(srcdir)/../corefile.c >core.tmp
	$(srcdir)/../../move-if-change core.tmp core.texi
	touch s-core
core.texi: s-core

s-elf: $(MKDOC) $(srcdir)/../elf.c $(srcdir)/doc.str
	./$(MKDOC) -f $(srcdir)/doc.str <$(srcdir)/../elf.c >elf.tmp
	$(srcdir)/../../move-if-change elf.tmp elf.texi
	touch s-elf
elf.texi: s-elf

s-elfcode: $(MKDOC) $(srcdir)/../elfcode.h $(srcdir)/doc.str
	./$(MKDOC) -f $(srcdir)/doc.str <$(srcdir)/../elfcode.h >elfcode.tmp
	$(srcdir)/../../move-if-change elfcode.tmp elfcode.texi
	touch s-elfcode
elfcode.texi: s-elfcode

s-mmo: $(MKDOC) $(srcdir)/../mmo.c $(srcdir)/doc.str
	./$(MKDOC) -f $(srcdir)/doc.str <$(srcdir)/../mmo.c >mmo.tmp
	$(srcdir)/../../move-if-change mmo.tmp mmo.texi
	touch s-mmo
mmo.texi: s-mmo

s-format: $(MKDOC) $(srcdir)/../format.c $(srcdir)/doc.str
	./$(MKDOC) -f $(srcdir)/doc.str <$(srcdir)/../format.c >format.tmp
	$(srcdir)/../../move-if-change format.tmp format.texi
	touch s-format
format.texi: s-format

s-libbfd: $(MKDOC) $(srcdir)/../libbfd.c $(srcdir)/doc.str
	./$(MKDOC) -f $(srcdir)/doc.str < $(srcdir)/../libbfd.c >libbfd.tmp
	$(srcdir)/../../move-if-change libbfd.tmp libbfd.texi
	touch s-libbfd
libbfd.texi: s-libbfd

s-opncls: $(MKDOC) $(srcdir)/../opncls.c $(srcdir)/doc.str
	./$(MKDOC) -f $(srcdir)/doc.str  <$(srcdir)/../opncls.c >opncls.tmp
	$(srcdir)/../../move-if-change opncls.tmp opncls.texi
	touch s-opncls
opncls.texi: s-opncls

s-reloc: $(MKDOC) $(srcdir)/../reloc.c $(srcdir)/doc.str
	./$(MKDOC) -f $(srcdir)/doc.str <$(srcdir)/../reloc.c >reloc.tmp
	$(srcdir)/../../move-if-change reloc.tmp reloc.texi
	touch s-reloc
reloc.texi: s-reloc

s-section: $(MKDOC) $(srcdir)/../section.c $(srcdir)/doc.str
	./$(MKDOC) -f $(srcdir)/doc.str <$(srcdir)/../section.c >section.tmp
	$(srcdir)/../../move-if-change section.tmp section.texi
	touch s-section
section.texi: s-section

s-syms: $(MKDOC) $(srcdir)/../syms.c $(srcdir)/doc.str
	./$(MKDOC) -f $(srcdir)/doc.str <$(srcdir)/../syms.c >syms.tmp
	$(srcdir)/../../move-if-change syms.tmp syms.texi
	touch s-syms
syms.texi: s-syms

s-targets: $(MKDOC) $(srcdir)/../targets.c $(srcdir)/doc.str
	./$(MKDOC) -f $(srcdir)/doc.str <$(srcdir)/../targets.c >targets.tmp
	$(srcdir)/../../move-if-change targets.tmp targets.texi
	touch s-targets
targets.texi: s-targets

s-init: $(MKDOC) $(srcdir)/../init.c $(srcdir)/doc.str
	./$(MKDOC) -f $(srcdir)/doc.str <$(srcdir)/../init.c >init.tmp
	$(srcdir)/../../move-if-change init.tmp init.texi
	touch s-init
init.texi: s-init

s-hash: $(MKDOC) $(srcdir)/../hash.c $(srcdir)/doc.str
	./$(MKDOC) -f $(srcdir)/doc.str <$(srcdir)/../hash.c >hash.tmp
	$(srcdir)/../../move-if-change hash.tmp hash.texi
	touch s-hash
hash.texi: s-hash

s-linker: $(MKDOC) $(srcdir)/../linker.c $(srcdir)/doc.str
	./$(MKDOC) -f $(srcdir)/doc.str <$(srcdir)/../linker.c >linker.tmp
	$(srcdir)/../../move-if-change linker.tmp linker.texi
	touch s-linker
linker.texi: s-linker

LIBBFD_H_DEP = \
	$(srcdir)/../libbfd-in.h	\
	$(srcdir)/../init.c		\
	$(srcdir)/../libbfd.c		\
	$(srcdir)/../cache.c		\
	$(srcdir)/../reloc.c		\
	$(srcdir)/../archures.c		\
	$(srcdir)/../elf.c		\
	$(srcdir)/header.sed		\
	$(srcdir)/proto.str		\
	$(MKDOC)

libbfd.h: $(LIBBFD_H_DEP)
	echo "$(LIBBFD_H_DEP)" | sed -f $(srcdir)/header.sed > $@
	for file in $(LIBBFD_H_DEP); do \
	  case $$file in \
	    *-in.h) cat $$file >> $@ ;; \
	    */header.sed) break ;; \
	    *)	echo $$file | sed -e 's,.*/,,' -e 's,^,/* Extracted from ,' \
				-e 's,$$,.  */,' >> $@ ; \
		./$(MKDOC) -i -f $(srcdir)/proto.str < $$file >> $@ ;; \
	  esac; \
	done

LIBCOFF_H_DEP = \
	$(srcdir)/../libcoff-in.h	\
	$(srcdir)/../coffcode.h		\
	$(srcdir)/header.sed		\
	$(srcdir)/proto.str		\
	$(MKDOC)

libcoff.h: $(LIBCOFF_H_DEP)
	echo "$(LIBCOFF_H_DEP)" | sed -f $(srcdir)/header.sed > $@
	for file in $(LIBCOFF_H_DEP); do \
	  case $$file in \
	    *-in.h) cat $$file >> $@ ;; \
	    */header.sed) break ;; \
	    *)	echo $$file | sed -e 's,.*/,,' -e 's,^,/* Extracted from ,' \
				-e 's,$$,.  */,' >> $@ ; \
		./$(MKDOC) -i -f $(srcdir)/proto.str < $$file >> $@ ;; \
	  esac; \
	done

BFD_H_DEP = \
	$(srcdir)/../bfd-in.h 		\
	$(srcdir)/../init.c 		\
	$(srcdir)/../opncls.c 		\
	$(srcdir)/../libbfd.c 		\
	$(srcdir)/../section.c 		\
	$(srcdir)/../archures.c		\
	$(srcdir)/../reloc.c		\
	$(srcdir)/../syms.c		\
	$(srcdir)/../bfd.c		\
	$(srcdir)/../archive.c		\
	$(srcdir)/../corefile.c		\
	$(srcdir)/../targets.c		\
	$(srcdir)/../format.c		\
	$(srcdir)/header.sed		\
	$(srcdir)/proto.str		\
	$(srcdir)/../version.h		\
	$(MKDOC)

bfd.h: $(BFD_H_DEP)
	echo "$(BFD_H_DEP)" | sed -f $(srcdir)/header.sed > $@
	for file in $(BFD_H_DEP); do \
	  case $$file in \
	    *-in.h) cat $$file >> $@ ;; \
	    */header.sed) break ;; \
	    *)	echo $$file | sed -e 's,.*/,,' -e 's,^,/* Extracted from ,' \
				-e 's,$$,.  */,' >> $@ ; \
		./$(MKDOC) -f $(srcdir)/proto.str < $$file >> $@ ;; \
	  esac; \
	done
	echo "#ifdef __cplusplus" >> $@
	echo "}" >> $@
	echo "#endif" >> $@
	echo "#endif" >> $@

noinst_TEXINFOS = bfdint.texi

MOSTLYCLEANFILES = $(MKDOC) *.o

CLEANFILES = s-* *.p *.ip

DISTCLEANFILES = bfd.?? bfd.??? bfd.h libbfd.h libcoff.h texput.log

MAINTAINERCLEANFILES = $(DOCFILES)

# We want install to imply install-info as per GNU standards, despite the
# cygnus option.
install: install-info
