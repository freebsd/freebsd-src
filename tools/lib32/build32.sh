#! /bin/sh
# $FreeBSD$
#
# This script is for running on a self-hosted amd64 machine, with an up-to-date
# world and toolchain etc.  ie: the installed world is assumed to match the sources.
# It is rude, crude and brutal.  But its the only option for now.
#
# Its purpose is to build a 32 bit library set and a ld-elf32.so.1.

# XXX beware.. some of the library targets have no way to disable
# XXX installation of includes.  ie: it will re-install some files in
# XXX /usr/include for you.

mkdir -p /lib32
mkdir -p /usr/lib32
mkdir -p /usr/local/lib32
mkdir -p /usr/X11R6/lib32

export MAKEOBJDIRPREFIX=/tmp/i386
mkdir -p $MAKEOBJDIRPREFIX

# Set up an obj tree
chflags -R noschg $MAKEOBJDIRPREFIX
rm -rf $MAKEOBJDIRPREFIX
mkdir -p $MAKEOBJDIRPREFIX

CCARGS="-m32 -march=athlon-xp -msse2 -mfancy-math-387 -I/tmp/i386/root/usr/include -L/usr/lib32 -B/usr/lib32"
CXXARGS="-m32 -march=athlon-xp -msse2 -mfancy-math-387 -I/tmp/i386/root/usr/include/c++/3.3 -I/tmp/i386/root/usr/include -L/usr/lib32 -B/usr/lib32"

# and a place to put the alternate include tree into.
mkdir -p $MAKEOBJDIRPREFIX/root

export MACHINE_ARCH=i386
export DESTDIR=$MAKEOBJDIRPREFIX/root

make -s hierarchy

# Now build includes
make -s obj
make -s includes

unset MACHINE_ARCH
unset DESTDIR

# libncurses needs a build-tools pass first.  I wish build-tools was a recursive target.
(cd lib/libncurses; make -s build-tools)

# Now the libraries.  This doesn't work for gnuregex yet. hence -k.
# libbind is just an internal target, ignore it.
export LIBDIR=/usr/lib32
export SHLIBDIR=/usr/lib32
export MACHINE_ARCH=i386
export CC="cc $CCARGS"
export CXX="c++ $CXXARGS"
export LD="ld -m elf_i386_fbsd -Y P,/usr/lib32" 
make -s -DNO_BIND -DNOMAN -DNODOC -DNOINFO -k libraries

# Hack to fix gnuregex which does hacks to the -I path based on $DESTDIR.  But, we cannot
# use DESTDIR during the libraries target, because we're just using alternate includes, not
# an alternate install directory.
unset CC
export CC="cc -I/tmp/i386/root/usr/include/gnu $CCARGS"
(cd gnu/lib/libregex; make -k -DNOMAN -DNODOC -DNOINFOall install)

# and now that we have enough libraries, build ld-elf32.so.1
cd libexec/rtld-elf
export PROG=ld-elf32.so.1
unset CC
export CC="cc $CCARGS -DCOMPAT_32BIT"
make -s -DNOMAN -DNODOC -DNOINFO -k obj
make -s -DNOMAN -DNODOC -DNOINFO -k depend
make -s -DNOMAN -DNODOC -DNOINFO -k 
make -s -DNOMAN -DNODOC -DNOINFO -k install
