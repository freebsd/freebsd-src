# $Id: Makefile.am,v 1.16 1999/07/26 12:38:01 joda Exp $

include $(top_srcdir)/Makefile.am.common

lib_LTLIBRARIES = libdes.la
libdes_la_LDFLAGS = -version-info 0:1:0

include_HEADERS = des.h md4.h md5.h sha.h

build_HEADERZ = $(include_HEADERS)

check_PROGRAMS = destest mdtest

TESTS = destest mdtest
CHECK_LOCAL = 
bin_PROGRAMS = des #rpw speed

des_SOURCES = des.c des_ver.h

LDADD = $(lib_LTLIBRARIES)

libdes_la_SOURCES =	\
	cbc3_enc.c	\
	cbc_cksm.c	\
	cbc_enc.c	\
	cfb64ede.c	\
	cfb64enc.c	\
	cfb_enc.c	\
	des_enc.c	\
	des_locl.h	\
	ecb3_enc.c	\
	ecb_enc.c	\
	ede_enc.c	\
	enc_read.c	\
	enc_writ.c	\
	fcrypt.c	\
	hash.h		\
	key_par.c	\
	md4.c		\
	md5.c		\
	ncbc_enc.c	\
	ofb64ede.c	\
	ofb64enc.c	\
	ofb_enc.c	\
	pcbc_enc.c	\
	podd.h		\
	qud_cksm.c	\
	read_pwd.c	\
	rnd_keys.c	\
	set_key.c	\
	sha.c		\
	sk.h		\
	spr.h		\
	str2key.c	\
	xcbc_enc.c

EXTRA_libdes_la_SOURCES = dllmain.c passwd_dialog.aps passwd_dialog.clw \
	passwd_dialog.rc passwd_dialog.res passwd_dlg.c passwd_dlg.h resource.h

## this is an awful lot of junk, but it's just as well to include everything
EXTRA_DIST =		\
	COPYRIGHT	\
	DES.pm		\
	DES.pod		\
	DES.xs		\
	FILES		\
	Imakefile	\
	KERBEROS	\
	MODES.DES	\
	Makefile.PL	\
	Makefile.ssl	\
	Makefile.uni	\
	PC1		\
	PC2		\
	VERSION		\
	des.def		\
	des.dsp		\
	des.doc		\
	des.mak		\
	des.man		\
	des.org		\
	des.pl		\
	des_crypt.man	\
	des_locl.org	\
	des_opts.c	\
	doIP		\
	doPC1		\
	doPC2		\
	makefile.bc	\
	rand_key.c	\
	rpc_des.h	\
	rpc_enc.c	\
	shifts.pl	\
	supp.c		\
	testdes.pl	\
	times		\
	typemap		\
	version.h	\
	vms.com

asm_files = des-som2.pl des-som3.pl des586.pl des686.pl desboth.pl \
	dx86-cpp.s dx86unix.cpp readme win32.asm win32.obj win32.uu x86ms.pl \
	x86unix.pl

dist-hook:
	$(mkinstalldirs) $(distdir)/t
	$(INSTALL_DATA) $(srcdir)/t/perl $(distdir)/t
	$(INSTALL_DATA) $(srcdir)/t/test $(distdir)/t
	$(mkinstalldirs) $(distdir)/asm
	(cd $(srcdir)/asm && tar cf - $(asm_files)) \
		| (cd $(distdir)/asm; tar xf -)

