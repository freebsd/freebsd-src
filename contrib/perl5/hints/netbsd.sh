# hints/netbsd.sh
#
# talk to mrg@eterna.com.au if you want to change this file.
#
# netbsd keeps dynamic loading dl*() functions in /usr/lib/crt0.o,
# so Configure doesn't find them (unless you abandon the nm scan).
# this should be *just* 0.9 below as netbsd 0.9a was the first to
# introduce shared libraries.  however, they don't work/build on
# pmax, powerpc and alpha ports correctly, yet.

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
	case `uname -m` in
	alpha|powerpc|pmax)
		d_dlopen=$undef
		;;
# this doesn't work (yet).
#	alpha)
#		d_dlopen=$define
#		d_dlerror=$define
#		cccdlflags="-DPIC -fPIC $cccdlflags"
#		lddlflags="-shared $lddlflags"
#		;;
	*)
		d_dlopen=$define
		d_dlerror=$define
# we use -fPIC here because -fpic is *NOT* enough for some of the
# extensions like Tk on some netbsd platforms (the sparc is one)
		cccdlflags="-DPIC -fPIC $cccdlflags"
		lddlflags="-Bforcearchive -Bshareable $lddlflags"
		;;
	esac
	;;
esac
# netbsd 1.3 linker warns about setr[gu]id being deprecated.
# (setregid, setreuid, preferred?)
case "$osvers" in
1.3|1.3*)
	d_setrgid="$undef"
	d_setruid="$undef"
	;;
esac

# netbsd had these but they don't really work as advertised, in the
# versions listed below.  if they are defined, then there isn't a
# way to make perl call setuid() or setgid().  if they aren't, then
# ($<, $>) = ($u, $u); will work (same for $(/$)).  this is because
# you can not change the real userid of a process under 4.4BSD.
# netbsd fixed this in 1.2A.
case "$osvers" in
0.9*|1.0*|1.1*|1.2_*|1.2|1.2.*)
	d_setregid="$undef"
	d_setreuid="$undef"
	d_setrgid="$undef"
	d_setruid="$undef"
	;;
esac
# netbsd 1.3 linker warns about setr[gu]id being deprecated.
# (setregid, setreuid, preferred?)
case "$osvers" in
1.3|1.3*)
	d_setrgid="$undef"
	d_setruid="$undef"
	;;
esac

# vfork is ok on NetBSD.
case "$usevfork" in
'') usevfork=true ;;
esac
