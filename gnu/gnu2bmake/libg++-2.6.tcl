#!/usr/local/bin/tclsh
#
# ----------------------------------------------------------------------------
# "THE BEER-WARE LICENSE" (Revision 42):
# <phk@login.dkuug.dk> wrote this file.  As long as you retain this notice you
# can do whatever you want with this stuff. If we meet some day, and you think
# this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
# ----------------------------------------------------------------------------
#
# $Id$
#

source gnu2bmake.tcl

#######################################################################
# Parameters to tweak
########
set sdir /a/phk/libg++-2.6
set ddir /a/phk/libg++26

#######################################################################
# Do the stunt
########
sh "cd $sdir ; sh configure i386--bsd"

sh "cd $sdir/libiberty ; make needed-list"
set l_ib [find_source $sdir/libiberty \
	[zap_suffix \
	    [makefile_macro LIBIBERTY_OBJECTS_TO_GET \
	    $sdir/libiberty $sdir/libg++/Makefile] \
	] \
	{.cc .C .c}]
set l_io [find_source $sdir/libio [zap_suffix [makefile_macro \
	LIBIOSTREAM_OBJECTS $sdir/libio]] {.cc .C .c}]

set l_plus [find_source $sdir/libg++/src \
	[zap_suffix [makefile_macro OBJS $sdir/libg++/src]] {.cc .C .c}]

set l_ioh ""
foreach i [zap_suffix $l_io] {
    if {[file exists $sdir/libio/${i}.h]} { lappend l_ioh ${i}.h }
}
set l_plush ""
foreach i [zap_suffix $l_plus] {
    if {[file exists $sdir/libg++/src/${i}.h]} { lappend l_plush ${i}.h }
}

# do ~
sh "rm -rf $ddir"
sh "mkdir $ddir $ddir/libg++ $ddir/libio $ddir/libiberty $ddir/include"

copy_l $sdir/libiberty $ddir/libiberty $l_ib
copy_l $sdir/libiberty $ddir/include {config.h}
copy_l $sdir/libio $ddir/libio $l_io
copy_l $sdir/libio $ddir/include $l_ioh
copy_l $sdir/libio $ddir/include {_G_config.h libioP.h floatio.h strfile.h 
	iostreamP.h libio.h iolibio.h}
copy_l $sdir/libg++/src $ddir/libg++ $l_plus
copy_l $sdir/libg++/src $ddir/include $l_plush
copy_l $sdir/libg++/src $ddir/include {defines.h std.h bitprims.h Integer.hP
	bitdo1.h bitdo2.h Pix.h}
copy_l $sdir/include $ddir/include {ansidecl.h libiberty.h}

set f [open $ddir/Makefile w]
puts $f "#\n# \$FreeBSD\$\n#\n"
puts $f "SRCS=\t$l_ib"
puts $f "SRCS+=\t$l_io"
puts $f "SRCS+=\t$l_plus"
puts $f "LIB=\tlibg++"
puts $f "NOMAN=\tnoman"
puts $f "CFLAGS+=\t-nostdinc -I\${.CURDIR}/include -I/usr/include"
puts $f "CXXFLAGS+=\t-fexternal-templates"
puts $f ".PATH:\t\${.CURDIR}/libiberty \${.CURDIR}/libio \${.CURDIR}/libg++"
puts $f {
beforeinstall:
	@-if [ ! -d ${DESTDIR}/usr/include/g++ ]; then \
		mkdir ${DESTDIR}/usr/include/g++; \
		chown ${BINOWN}.${BINGRP} ${DESTDIR}/usr/include/g++; \
		chmod 755 ${DESTDIR}/usr/include/g++; \
	fi
	@(cd include ; for j in *.h; do \
		cmp -s $$j ${DESTDIR}/usr/include/g++/$$j || \
		install -c -o ${BINOWN} -g ${BINGRP} -m 444 $$j \
			${DESTDIR}/usr/include/$$j; \
	done)
}
puts $f ".include <bsd.lib.mk>"
close $f
exit 0
