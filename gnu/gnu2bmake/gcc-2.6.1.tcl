#!/usr/local/bin/tclsh
#
# ----------------------------------------------------------------------------
# "THE BEER-WARE LICENSE" (Revision 42):
# <phk@login.dkuug.dk> wrote this file.  As long as you retain this notice you
# can do whatever you want with this stuff. If we meet some day, and you think
# this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
# ----------------------------------------------------------------------------
#
# $FreeBSD$
#
# Good for 2.6.1 and 2.6.2

source gnu2bmake.tcl

#######################################################################
# Parameters to tweak
########
set sdir /freebsd/gcc-2.6.2
set ddir /freebsd/cc262

#######################################################################
# Do the stunt
########
sh "cd $sdir ; sh configure i386--freebsd"

# .h files on their way to ~/include
set l_include {config tm pcp tree input c-lex c-tree flags machmode real 
	rtl c-parse c-gperf function defaults convert obstack insn-attr
	bytecode bc-emit insn-flags expr insn-codes regs hard-reg-set
	insn-config loop recog bc-typecd bc-opcode bc-optab typeclass
	output basic-block reload integrate conditions bytetypes bi-run
	bc-arity multilib stack}

# other files on their way to ~/include
set l_include_x {tree.def machmode.def rtl.def modemap.def bc-typecd.def}

# .h files going into ~/include/i386
set l_include_i386 {perform gstabs gas bsd i386 unix }

# .c source for cpp
set l_cpp {cccp cexp version}

# .c source for cc1
set l_cc1 [zap_suffix [makefile_macro C_OBJS $sdir]]
append l_cc1 " " [zap_suffix [makefile_macro OBJS $sdir]]
append l_cc1 " " [zap_suffix [makefile_macro BC_OBJS $sdir]]

# .c source for cc
set l_cc {gcc version}
append l_cc " " [zap_suffix [makefile_macro OBSTACK $sdir]]

# .c source for c++
set l_cplus [zap_suffix [makefile_macro OBSTACK $sdir]]

# .c source for c++ from "cp" subdir
set l_cplus_cp {g++}

# .c source for cc1plus
set l_cc1plus {c-common}
append l_cc1plus " " [zap_suffix [makefile_macro OBJS $sdir]]
append l_cc1plus " " [zap_suffix [makefile_macro BC_OBJS $sdir]]

# .c source for cc1plus from "cp" subdir
set l_cc1plus_cp {}
append l_cc1plus_cp " " [zap_suffix [makefile_macro CXX_OBJS $sdir/cp]]

# .h file for cc1plus from "cp" subdir
set l_cc1plus_h {lex parse cp-tree decl class hash}

# other file for cc1plus from "cp" subdir
set l_cc1plus_x {tree.def input.c}

# All files used more than once go into the lib.
set l_common [common_set $l_cpp $l_cc1 $l_cc $l_cc1plus $l_cplus]
set l_cpp [reduce_by $l_cpp $l_common]
set l_cc1 [reduce_by $l_cc1 $l_common]
set l_cc [reduce_by $l_cc $l_common]
set l_cplus [reduce_by $l_cplus $l_common]
set l_cc1plus [reduce_by $l_cc1plus $l_common]

# functions in libgcc1
set l_libgcc1 [makefile_macro LIB1FUNCS $sdir]
# functions in libgcc2
set l_libgcc2 [makefile_macro LIB2FUNCS $sdir]
# .c files in libgcc
set l_libgcc {libgcc1.c libgcc2.c}
# .h files in libgcc
set l_libgcc_h {tconfig longlong glimits gbl-ctors}

set version [makefile_macro version $sdir]
set target [makefile_macro target $sdir]

# do ~
sh "rm -rf $ddir"
sh "mkdir $ddir"
set f [open $ddir/Makefile.inc w]
puts $f "#\n# \$FreeBSD$\n#\n"
puts $f "CFLAGS+=\t-I\${.CURDIR} -I\${.CURDIR}/../include"
puts $f "CFLAGS+=\t-Dbsd4_4"
puts $f "CFLAGS+=\t-DGCC_INCLUDE_DIR=\\\"FOO\\\""
puts $f "CFLAGS+=\t-DTOOL_INCLUDE_DIR=\\\"FOO\\\""
puts $f "CFLAGS+=\t-DGPLUSPLUS_INCLUDE_DIR=\\\"FOO\\\""
puts $f "CFLAGS+=\t-DDEFAULT_TARGET_VERSION=\\\"$version\\\""
puts $f "CFLAGS+=\t-DDEFAULT_TARGET_MACHINE=\\\"$target\\\""
puts $f "CFLAGS+=\t-DMD_EXEC_PREFIX=\\\"/usr/libexec/\\\""
puts $f "CFLAGS+=\t-DSTANDARD_STARTFILE_PREFIX=\\\"/usr/lib\\\""
puts $f "CFLAGS+=\t-DGCC_NAME=\\\"cc\\\""
puts $f ""
puts $f ".if exists(\${.CURDIR}/../cc_int/obj)"
puts $f "LIBDESTDIR=\t\${.CURDIR}/../cc_int/obj"
puts $f ".else"
puts $f "LIBDESTDIR=\t\${.CURDIR}/../cc_int"
puts $f ".endif"
puts $f ""

puts $f "# XXX LDDESTDIR isn't a directory and there is no standard name for the dir"
puts $f "LDDESTDIR=\t-L\${LIBDESTDIR}"
puts $f ".if defined(SHARED_LIBCC_INT)"
puts $f "LIBCC_INT=\t\${LIBDESTDIR}/libcc_int.so.262.0"
puts $f ".else"
puts $f "LIBCC_INT=\t\${LIBDESTDIR}/libcc_int.a"
puts $f ".endif"
close $f

set f [open $ddir/Makefile w]
puts $f "#\n# \$FreeBSD$\n#\n"
puts $f "PGMDIR=\tcc_int cpp cc1 cc cc1plus c++ f77 libgcc"
puts $f "SUBDIR=\t\$(PGMDIR)"
puts $f "\n.include <bsd.subdir.mk>"
close $f

# do ~/legal
sh "mkdir $ddir/legal"
sh "cp $sdir/gen-*.c $sdir/md $ddir/legal"
set f [open $ddir/README w]
puts $f {
$FreeBSD$

This directory contains gcc in a form that uses "bmake" makefiles.
This is not the place you want to start, if you want to hack gcc.
we have included everything here which is part of the source-code
of gcc, but still, don't use this as a hacking-base.

If you suspect a problem with gcc, or just want to hack it in general,
get a complete gcc-X.Y.Z.tar.gz from somewhere, and use that.

Please look in the directory src/gnu/gnu2bmake to find the tools
to generate these files.

Thankyou.
}

# do ~/libgcc
sh "mkdir $ddir/libgcc"
set f [open $ddir/libgcc/Makefile w]
puts $f "#\n# \$FreeBSD$\n#\n"
puts $f "LIB=\tgcc"
puts $f "INSTALL_PIC_ARCHIVE=\tyes"
puts $f "SHLIB_MAJOR=\t261"
puts $f "SHLIB_MINOR=\t0"
puts $f ""
puts $f "LIB1OBJS=\t[add_suffix $l_libgcc1 .o]"
puts $f "LIB2OBJS=\t[add_suffix $l_libgcc2 .o]"
puts $f {
OBJS= ${LIB1OBJS} ${LIB2OBJS}
LIB1SOBJS=${LIB1OBJS:.o=.so}
LIB2SOBJS=${LIB2OBJS:.o=.so}
P1OBJS=${LIB1OBJS:.o=.po}
P2OBJS=${LIB2OBJS:.o=.po}

${LIB1OBJS}: libgcc1.c
	${CC} -c ${CFLAGS} -DL${.PREFIX} -o ${.TARGET} ${.CURDIR}/libgcc1.c
	@${LD} -x -r ${.TARGET}
	@mv a.out ${.TARGET}

${LIB2OBJS}: libgcc2.c
	${CC} -c ${CFLAGS} -DL${.PREFIX} -o ${.TARGET} ${.CURDIR}/libgcc2.c
	@${LD} -x -r ${.TARGET}
	@mv a.out ${.TARGET}

.if !defined(NOPIC)
${LIB1SOBJS}: libgcc1.c
	${CC} -c -fpic ${CFLAGS} -DL${.PREFIX} -o ${.TARGET} ${.CURDIR}/libgcc1.c

${LIB2SOBJS}: libgcc2.c
	${CC} -c -fpic ${CFLAGS} -DL${.PREFIX} -o ${.TARGET} ${.CURDIR}/libgcc2.c
.endif

.if !defined(NOPROFILE)
${P1OBJS}: libgcc1.c
	${CC} -c -p ${CFLAGS} -DL${.PREFIX} -o ${.TARGET} ${.CURDIR}/libgcc1.c

${P2OBJS}: libgcc2.c
	${CC} -c -p ${CFLAGS} -DL${.PREFIX} -o ${.TARGET} ${.CURDIR}/libgcc2.c
.endif

.include <bsd.lib.mk>
}
close $f
copy_c $sdir $ddir/libgcc $l_libgcc

# do ~/include
sh "mkdir $ddir/include"
copy_l $sdir $ddir/include [add_suffix $l_include .h]
copy_l $sdir $ddir/include $l_include_x
copy_l $sdir $ddir/include [add_suffix $l_libgcc_h .h]

# do ~/include/i386
sh "mkdir $ddir/include/i386"
copy_l $sdir/config/i386 $ddir/include/i386 [add_suffix $l_include_i386 .h]

# do ~/cc_int
mk_lib $ddir cc_int [add_suffix $l_common .c] {
	"NOPROFILE=\tyes"
	".if defined(SHARED_LIBCC_INT)"
	"INTERNALLIB="
	"SHLIB_MAJOR=262"
	"SHLIB_MINOR=0"
	".else"
	"\ninstall:\n\t@true"
	".endif"
}
copy_c $sdir $ddir/cc_int $l_common

# do ~/cpp
mk_prog $ddir cpp [add_suffix $l_cpp .c] { 
	"BINDIR=\t/usr/libexec" 
	".PATH:\t\${.CURDIR}/../cc_int"
	"SRCS+=\tobstack.c version.c"
}
copy_c $sdir $ddir/cpp $l_cpp
cp $sdir/cccp.1 $ddir/cpp/cpp.1

# do ~/c++
mk_prog $ddir c++ [add_suffix "$l_cplus $l_cplus_cp" .c] {
	"BINDIR=\t/usr/bin"
	"LINKS=\t\${BINDIR}/c++ \${BINDIR}/g++"
	"NOMAN=\t1"
}
copy_c $sdir $ddir/c++ $l_cplus
copy_c $sdir/cp $ddir/c++ $l_cplus_cp

# do ~/cc
mk_prog $ddir cc [add_suffix $l_cc .c] {
	"BINDIR=\t/usr/bin"
	".PATH: \${.CURDIR}/../cc_int"
	"SRCS+=\tobstack.c version.c"
	"LINKS=\t\${BINDIR}/cc \${BINDIR}/gcc"
}
copy_c $sdir $ddir/cc $l_cc
cp $sdir/gcc.1 $ddir/cc/cc.1

# do ~/cc1
mk_prog $ddir cc1 [add_suffix $l_cc1 .c] {
	"BINDIR=\t/usr/libexec"
	"NOMAN=\t1"
	"DPADD+=\t\${LIBCC_INT} \${LIBGNUMALLOC}"
	"LDADD+=\t-lcc_int -lgnumalloc"
}
copy_c $sdir $ddir/cc1 $l_cc1

# do ~/cc1plus
mk_prog $ddir cc1plus [add_suffix "$l_cc1plus_cp $l_cc1plus" .c] {
	"BINDIR=\t/usr/libexec"
	"NOMAN=\t1"
	"DPADD+=\t\${LIBCC_INT} \${LIBGNUMALLOC}"
	"LDADD+=\t-lcc_int -lgnumalloc"
}
copy_l $sdir/cp $ddir/cc1plus $l_cc1plus_x
copy_c $sdir $ddir/cc1plus $l_cc1plus
copy_c $sdir/cp $ddir/cc1plus $l_cc1plus_cp
copy_l $sdir/cp $ddir/cc1plus [add_suffix $l_cc1plus_h .h]

exit 0
