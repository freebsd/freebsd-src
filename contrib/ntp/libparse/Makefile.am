#AUTOMAKE_OPTIONS = ../util/ansi2knr no-dependencies
AUTOMAKE_OPTIONS = ../util/ansi2knr
noinst_LIBRARIES = @MAKE_LIBPARSE@ @MAKE_LIBPARSE_KERNEL@
EXTRA_LIBRARIES = libparse.a libparse_kernel.a
EXTRA_PROGRAMS = parsestreams parsesolaris
noinst_PROGRAMS = @MAKE_PARSEKMODULE@
CLEANFILES = libparse.a libparse_kernel.a

libparse_a_SOURCES = parse.c \
                     parse_conf.c \
                     clk_meinberg.c \
                     clk_schmid.c \
		     clk_rawdcf.c \
		     clk_trimtsip.c \
	             clk_dcf7000.c \
                     clk_trimtaip.c \
                     clk_rcc8000.c \
                     clk_hopf6021.c \
	             clk_computime.c \
		     clk_wharton.c \
		     clk_varitext.c \
                     data_mbg.c \
		     info_trimble.c \
		     trim_info.c

libparse_kernel_a_SOURCES = 
libparse_kernel_a_LIBADD = kparse$U.o \
			    kparse_conf$U.o \
			    kclk_computime$U.o \
			    kclk_dcf7000$U.o \
			    kclk_hopf6021$U.o \
			    kclk_meinberg$U.o \
			    kclk_rawdcf$U.o \
			    kclk_rcc8000$U.o \
			    kclk_schmid$U.o \
			    kclk_trimtaip$U.o \
			    kclk_trimtsip$U.o \
			    kclk_varitext$U.o \
			    kclk_wharton$U.o

INCLUDES = -I$(top_srcdir)/include -I$(top_srcdir)/kernel
ETAGS_ARGS = Makefile.am

EXTRA_DIST = parsesolaris.c parsestreams.c mkinfo_scmd.sed mkinfo_rcmd.sed info_trimble.c

#
# create info_trimble.c
#
info_trimble.c: $(top_srcdir)/include/trimble.h mkinfo_rcmd.sed mkinfo_scmd.sed
	@rm -f info_trimble.c
	sed -n -f mkinfo_scmd.sed $(top_srcdir)/include/trimble.h  > info_trimble.c || rm -f info_trimble.c
	sed -n -f mkinfo_rcmd.sed $(top_srcdir)/include/trimble.h >> info_trimble.c || rm -f info_trimble.c

kclk_computime$U.o: clk_computime$U.c
	$(COMPILE) -DPARSESTREAM -c $< -o $@

kclk_dcf7000$U.o: clk_dcf7000$U.c
	$(COMPILE) -DPARSESTREAM -c $< -o $@

kclk_hopf6021$U.o: clk_hopf6021$U.c
	$(COMPILE) -DPARSESTREAM -c $< -o $@

kclk_meinberg$U.o: clk_meinberg$U.c
	$(COMPILE) -DPARSESTREAM -c $< -o $@

kclk_rawdcf$U.o: clk_rawdcf$U.c
	$(COMPILE) -DPARSESTREAM -c $< -o $@

kclk_rcc8000$U.o: clk_rcc8000$U.c
	$(COMPILE) -DPARSESTREAM -c $< -o $@

kclk_schmid$U.o: clk_schmid$U.c
	$(COMPILE) -DPARSESTREAM -c $< -o $@

kclk_trimtaip$U.o: clk_trimtaip$U.c
	$(COMPILE) -DPARSESTREAM -c $< -o $@

kclk_trimtsip$U.o: clk_trimtsip$U.c
	$(COMPILE) -DPARSESTREAM -c $< -o $@

kclk_varitext$U.o: clk_varitext$U.c
	$(COMPILE) -DPARSESTREAM -c $< -o $@

kclk_wharton$U.o: clk_wharton$U.c
	$(COMPILE) -DPARSESTREAM -c $< -o $@

kparse$U.o: parse$U.c
	$(COMPILE) -DPARSESTREAM -c $< -o $@

kparse_conf$U.o: parse_conf$U.c
	$(COMPILE) -DPARSESTREAM -c $< -o $@

parsestreams.loadable_module.o: $(parsestreams_OBJECTS) libparse_kernel.a ../libntp/libntp.a
	$(LD) -r -o $@ $(parsestreams_OBJECTS) libparse_kernel.a ../libntp/libntp.a

parse: $(parsesolaris_OBJECTS) libparse_kernel.a ../libntp/libntp.a
	$(LD) -r -o $@ $(parsesolaris_OBJECTS) libparse_kernel.a ../libntp/libntp.a

../libntp/libntp.a:
	cd ../libntp && $(MAKE)

parsesolaris.o: sys/systm.h

sys/systm.h:
	mkdir sys && \
	sed -e 's/ffs(long)/ffs(int)/' < /usr/include/sys/systm.h > sys/systm.h
