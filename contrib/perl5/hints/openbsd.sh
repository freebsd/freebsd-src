# hints/openbsd.sh
#
# hints file for OpenBSD; Todd Miller <millert@openbsd.org>
# Edited to allow Configure command-line overrides by
#  Andy Dougherty <doughera@lafcol.lafayette.edu>
#

# OpenBSD has a better malloc than perl...
test "$usemymalloc" || usemymalloc='n'

# Currently, vfork(2) is not a real win over fork(2) but this will
# change in a future release.
usevfork='true'

# setre?[ug]id() have been replaced by the _POSIX_SAVED_IDS versions
# in 4.4BSD.  Configure will find these but they are just emulated
# and do not have the same semantics as in 4.3BSD.
d_setregid='undef'
d_setreuid='undef'
d_setrgid='undef'
d_setruid='undef'

#
# Not all platforms support shared libs...
#
case `uname -m` in
alpha|mips|powerpc|vax)
	d_dlopen=$undef
	;;
*)
	d_dlopen=$define
	d_dlerror=$define
	# we use -fPIC here because -fpic is *NOT* enough for some of the
	# extensions like Tk on some OpenBSD platforms (ie: sparc)
	cccdlflags="-DPIC -fPIC $cccdlflags"
	lddlflags="-Bforcearchive -Bshareable $lddlflags"
	;;
esac

# OpenBSD doesn't need libcrypt but many folks keep a stub lib
# around for old NetBSD binaries.
libswanted=`echo $libswanted | sed 's/ crypt / /'`

# Configure can't figure this out non-interactively
d_suidsafe='define'

# cc is gcc so we can do better than -O
# Allow a command-line override, such as -Doptimize=-g
test "$optimize" || optimize='-O2'

# This script UU/usethreads.cbu will get 'called-back' by Configure 
# after it has prompted the user for whether to use threads.
cat > UU/usethreads.cbu <<'EOCBU'
case "$usethreads" in
$define|true|[yY]*)
	# any openbsd version dependencies with pthreads?
	libswanted="$libswanted pthread"
esac
EOCBU

# end
