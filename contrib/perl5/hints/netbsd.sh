# hints/netbsd.sh
#
# talk to packages@netbsd.org if you want to change this file.
#
# netbsd keeps dynamic loading dl*() functions in /usr/lib/crt0.o,
# so Configure doesn't find them (unless you abandon the nm scan).
# this should be *just* 0.9 below as netbsd 0.9a was the first to
# introduce shared libraries.

case "$archname" in
'')
    archname=`uname -m`-${osname}
    ;;
esac

case "$osvers" in
0.9|0.8*)
	usedl="$undef"
	;;
*)
	if [ -f /usr/libexec/ld.elf_so ]; then
		d_dlopen=$define
		d_dlerror=$define
		ccdlflags="-Wl,-E -Wl,-R${PREFIX}/lib $ccdlflags"
		cccdlflags="-DPIC -fPIC $cccdlflags"
		lddlflags="--whole-archive -shared $lddlflags"
	elif [ "`uname -m`" = "pmax" ]; then
# NetBSD 1.3 and 1.3.1 on pmax shipped an `old' ld.so, which will not work.
		d_dlopen=$undef
	elif [ -f /usr/libexec/ld.so ]; then
		d_dlopen=$define
		d_dlerror=$define
		ccdlflags="-Wl,-R${PREFIX}/lib $ccdlflags"
# we use -fPIC here because -fpic is *NOT* enough for some of the
# extensions like Tk on some netbsd platforms (the sparc is one)
		cccdlflags="-DPIC -fPIC $cccdlflags"
		lddlflags="-Bshareable $lddlflags"
	else
		d_dlopen=$undef
	fi
	;;
esac

# netbsd had these but they don't really work as advertised, in the
# versions listed below.  if they are defined, then there isn't a
# way to make perl call setuid() or setgid().  if they aren't, then
# ($<, $>) = ($u, $u); will work (same for $(/$)).  this is because
# you can not change the real userid of a process under 4.4BSD.
# netbsd fixed this in 1.3.2.
case "$osvers" in
0.9*|1.[012]*|1.3|1.3.1)
	d_setregid="$undef"
	d_setreuid="$undef"
	;;
esac

# These are obsolete in any netbsd.
d_setrgid="$undef"
d_setruid="$undef"

# there's no problem with vfork.
case "$usevfork" in
'') usevfork=true ;;
esac

# Pre-empt the /usr/bin/perl question of installperl.
installusrbinperl='n'

# Recognize the NetBSD packages collection.
# GDBM might be here.
test -d /usr/pkg/lib     && loclibpth="$loclibpth /usr/pkg/lib"
test -d /usr/pkg/include && locincpth="$locincpth /usr/pkg/include"
