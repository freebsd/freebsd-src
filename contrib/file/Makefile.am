# don't enforce GNU packaging standards
AUTOMAKE_OPTIONS = foreign no-dependencies

bin_PROGRAMS = file

data_DATA = magic magic.mime magic.mgc

MAGIC	 = @datadir@/magic
CPPFLAGS = -DMAGIC='"$(MAGIC)"' -DQUICK

man_MANS = file.1 magic.4

file_SOURCES = file.c apprentice.c fsmagic.c softmagic.c ascmagic.c \
	compress.c is_tar.c readelf.c print.c \
	file.h names.h patchlevel.h readelf.h tar.h

EXTRA_DIST = LEGAL.NOTICE MAINT PORTING Makefile.std magic2mime \
	Localstuff Header $(magic_FRAGMENTS) file.man magic.man

CLEANFILES = $(man_MANS) magic magic.mgc

magic: Header Localstuff $(magic_FRAGMENTS)
	cat $(srcdir)/Header $(srcdir)/Localstuff > $@
	for frag in $(magic_FRAGMENTS); do \
	  if test -f $(srcdir)/$$frag; then \
	    f=$(srcdir)/$$frag; \
	  else \
	    f=$$frag; \
	  fi; \
          cat $$f; \
	done >> $@

magic.mgc: magic file
	./file -C -m magic

file.1:	Makefile file.man
	@rm -f $@
	sed -e s@__CSECTION__@1@g \
	    -e s@__FSECTION__@4@g \
	    -e s@__VERSION__@${VERSION}@g \
	    -e s@__MAGIC__@${MAGIC}@g $(srcdir)/file.man > $@

magic.4: Makefile magic.man
	@rm -f $@
	sed -e s@__CSECTION__@1@g \
	    -e s@__FSECTION__@4@g \
	    -e s@__VERSION__@${VERSION}@g \
	    -e s@__MAGIC__@${MAGIC}@g $(srcdir)/magic.man > $@

magic_FRAGMENTS = Magdir/adventure Magdir/allegro Magdir/alliant \
    Magdir/alpha Magdir/amanda Magdir/amigaos Magdir/animation \
    Magdir/apl Magdir/apple Magdir/applix Magdir/archive Magdir/asterix \
    Magdir/att3b Magdir/audio Magdir/blender Magdir/blit Magdir/bsdi \
    Magdir/c-lang Magdir/chi Magdir/cisco Magdir/claris Magdir/clipper \
    Magdir/commands Magdir/compress Magdir/console Magdir/convex \
    Magdir/database Magdir/diamond Magdir/diff Magdir/digital \
    Magdir/dump Magdir/elf Magdir/encore Magdir/epoc Magdir/filesystems \
    Magdir/flash Magdir/fonts Magdir/frame Magdir/freebsd Magdir/fsav \
    Magdir/gimp Magdir/gnu Magdir/grace Magdir/hp Magdir/ibm370 \
    Magdir/ibm6000 Magdir/iff Magdir/images Magdir/intel \
    Magdir/interleaf Magdir/island Magdir/ispell Magdir/java \
    Magdir/jpeg Magdir/karma Magdir/lecter Magdir/lex Magdir/lif \
    Magdir/linux Magdir/lisp Magdir/mach Magdir/macintosh Magdir/magic \
    Magdir/mail.news Magdir/maple Magdir/mathematica Magdir/mcrypt \
    Magdir/mime Magdir/mips Magdir/mirage Magdir/mkid Magdir/mmdf Magdir/modem \
    Magdir/motorola Magdir/msdos Magdir/msvc Magdir/ncr Magdir/netbsd \
    Magdir/netscape Magdir/news Magdir/octave Magdir/olf Magdir/os2 \
    Magdir/os9 Magdir/osf1 Magdir/palm Magdir/pbm Magdir/pdf Magdir/pdp \
    Magdir/pgp Magdir/pkgadd Magdir/plus5 Magdir/printer Magdir/project \
    Magdir/psdbms Magdir/pyramid Magdir/python Magdir/riff Magdir/rpm \
    Magdir/rtf Magdir/sc Magdir/sccs Magdir/sendmail Magdir/sequent \
    Magdir/sgml Magdir/sniffer Magdir/softquad Magdir/spectrum \
    Magdir/sun Magdir/teapot Magdir/terminfo Magdir/tex Magdir/ti-8x \
    Magdir/timezone Magdir/troff Magdir/typeset Magdir/unknown \
    Magdir/uuencode Magdir/varied.out Magdir/vax Magdir/vicar Magdir/visx \
    Magdir/vms Magdir/vmware Magdir/wordperfect Magdir/xdelta Magdir/xenix \
    Magdir/zilog Magdir/zyxel
