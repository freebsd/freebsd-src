# hints/openbsd.sh
#
# hints file for OpenBSD; Todd Miller <millert@openbsd.org>
# Edited to allow Configure command-line overrides by
#  Andy Dougherty <doughera@lafcol.lafayette.edu>
#
# To build with distribution paths, use:
#	./Configure -des -Dopenbsd_distribution=defined
#

# OpenBSD has a better malloc than perl...
test "$usemymalloc" || usemymalloc='n'

# Currently, vfork(2) is not a real win over fork(2) but this will
# change starting with OpenBSD 2.7.
usevfork='true'

# setre?[ug]id() have been replaced by the _POSIX_SAVED_IDS versions
# in 4.4BSD.  Configure will find these but they are just emulated
# and do not have the same semantics as in 4.3BSD.
d_setregid=$undef
d_setreuid=$undef
d_setrgid=$undef
d_setruid=$undef

#
# Not all platforms support dynamic loading...
#
ARCH=`arch|sed 's/^OpenBSD.//'`
case "${ARCH}-${osvers}" in
alpha-*|mips-*|vax-*|powerpc-2.[0-7]|m88k-*)
	usedl=$undef
	;;
*)
	usedl=$define
	d_dlopen=$define
	d_dlerror=$define
	# we use -fPIC here because -fpic is *NOT* enough for some of the
	# extensions like Tk on some OpenBSD platforms (ie: sparc)
	cccdlflags="-DPIC -fPIC $cccdlflags"
	case "$osvers" in
	[01].*|2.[0-7]|2.[0-7].*)
		lddlflags="-Bshareable $lddlflags"
		;;
	*) # from 2.8 onwards
		ld=${cc:-cc}
		lddlflags="-shared -fPIC $lddlflags"
		;;
	esac
	;;
esac

#
# Tweaks for various versions of OpenBSD
#
case "$osvers" in
2.5)
	# OpenBSD 2.5 has broken odbm support
	i_dbm=$undef
	;;
esac

# OpenBSD doesn't need libcrypt but many folks keep a stub lib
# around for old NetBSD binaries.
libswanted=`echo $libswanted | sed 's/ crypt / /'`

# Configure can't figure this out non-interactively
d_suidsafe=$define

# cc is gcc so we can do better than -O
# Allow a command-line override, such as -Doptimize=-g
case "$ARCH" in
m88k)
   optimize='-O0'
   ;;
*)
   test "$optimize" || optimize='-O2'
   ;;
esac

# This script UU/usethreads.cbu will get 'called-back' by Configure 
# after it has prompted the user for whether to use threads.
cat > UU/usethreads.cbu <<'EOCBU'
case "$usethreads" in
$define|true|[yY]*)
	# any openbsd version dependencies with pthreads?
	ccflags="-pthread $ccflags"
	ldflags="-pthread $ldflags"
	libswanted="$libswanted pthread"
	# This is strange.
	usevfork="$undef"
esac
EOCBU

# When building in the OpenBSD tree we use different paths
# This is only part of the story, the rest comes from config.over
case "$openbsd_distribution" in
''|$undef|false) ;;
*)
	# We put things in /usr, not /usr/local
	prefix='/usr'
	prefixexp='/usr'
	sysman='/usr/share/man/man1'
	libpth='/usr/lib'
	glibpth='/usr/lib'
	# Local things, however, do go in /usr/local
	siteprefix='/usr/local'
	siteprefixexp='/usr/local'
	# Ports installs non-std libs in /usr/local/lib so look there too
	locincpth='/usr/local/include'
	loclibpth='/usr/local/lib'
	# Link perl with shared libperl
	if [ "$usedl" = "$define" -a -r shlib_version ]; then
		useshrplib=true
		libperl=`. ./shlib_version; echo libperl.so.${major}.${minor}`
	fi
	;;
esac

# end
