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

mkdir /lib32
mkdir /usr/lib32
mkdir /usr/local/lib32
mkdir /usr/X11R6/lib32

# Set up an obj tree
chflags -R noschg /tmp/i386
rm -rf /tmp/i386

CCARGS="-m32 -march=athlon-xp -msse2 -mfancy-math-387 -I/tmp/i386/root/usr/include -L/usr/lib32 -B/usr/lib32"
CXXARGS="-m32 -march=athlon-xp -msse2 -mfancy-math-387 -I/tmp/i386/root/usr/include/c++/3.3 -I/tmp/i386/root/usr/include -L/usr/lib32 -B/usr/lib32"

# and a place to put the alternate include tree into.
mkdir -p /tmp/i386/root
make -s MAKEOBJDIRPREFIX=/tmp/i386 DESTDIR=/tmp/i386/root MACHINE_ARCH=i386 hierarchy

# Now build includes
make -s MAKEOBJDIRPREFIX=/tmp/i386 DESTDIR=/tmp/i386/root MACHINE_ARCH=i386 obj
make -s MAKEOBJDIRPREFIX=/tmp/i386 DESTDIR=/tmp/i386/root MACHINE_ARCH=i386 includes

# libncurses needs a build-tools pass first.  I wish build-tools was a recursive target.
(cd lib/libncurses; make -s MAKEOBJDIRPREFIX=/tmp/i386 build-tools)

# Now the libraries.  This doesn't work for gnuregex yet. hence -k.
# libbind is just an internal target, ignore it.
make -s -DNO_BIND -DNOMAN -DNODOC -DNOINFO MAKEOBJDIRPREFIX=/tmp/i386 LIBDIR=/usr/lib32 SHLIBDIR=/usr/lib32 MACHINE_ARCH=i386 CC="cc $CCARGS" CXX="c++ $CXXARGS" LD="ld -m elf_i386_fbsd -Y P,/usr/lib32" -k libraries

# Hack to fix gnuregex which does hacks to the -I path based on $DESTDIR.  But, we cannot
# use DESTDIR during the libraries target, because we're just using alternate includes, not
# an alternate install directory.
(cd gnu/lib/libregex; make -s -DNOMAN -DNODOC -DNOINFO MAKEOBJDIRPREFIX=/tmp/i386 LIBDIR=/usr/lib32 SHLIBDIR=/usr/lib32 MACHINE_ARCH=i386 CC="cc -I/tmp/i386/root/usr/include/gnu $CCARGS" CXX="c++ $CXXARGS" LD="ld -m elf_i386_fbsd -Y P,/usr/lib32" all install)

# and now that we have enough libraries, build ld-elf32.so.1
cd libexec/rtld-elf
make -s -DNOMAN -DNODOC -DNOINFO PROG=ld-elf32.so.1 MAKEOBJDIRPREFIX=/tmp/i386 LIBDIR=/usr/lib32 SHLIBDIR=/usr/lib32 MACHINE_ARCH=i386 CC="cc $CCARGS -DCOMPAT_32BIT" LD="ld -m elf_i386_fbsd -Y P,/usr/lib32" obj
make -s -DNOMAN -DNODOC -DNOINFO PROG=ld-elf32.so.1 MAKEOBJDIRPREFIX=/tmp/i386 LIBDIR=/usr/lib32 SHLIBDIR=/usr/lib32 MACHINE_ARCH=i386 CC="cc $CCARGS -DCOMPAT_32BIT" LD="ld -m elf_i386_fbsd -Y P,/usr/lib32" depend
make -s -DNOMAN -DNODOC -DNOINFO PROG=ld-elf32.so.1 MAKEOBJDIRPREFIX=/tmp/i386 LIBDIR=/usr/lib32 SHLIBDIR=/usr/lib32 MACHINE_ARCH=i386 CC="cc $CCARGS -DCOMPAT_32BIT" LD="ld -m elf_i386_fbsd -Y P,/usr/lib32" 
make -s -DNOMAN -DNODOC -DNOINFO PROG=ld-elf32.so.1 MAKEOBJDIRPREFIX=/tmp/i386 LIBDIR=/usr/lib32 SHLIBDIR=/usr/lib32 MACHINE_ARCH=i386 CC="cc $CCARGS -DCOMPAT_32BIT" LD="ld -m elf_i386_fbsd -Y P,/usr/lib32" install
