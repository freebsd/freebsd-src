# hints/aux_3.sh
#
# Improved by Jake Hamby <jehamby@lightside.com> to support both Apple CC
# and GNU CC.  Tested on A/UX 3.1.1 with GCC 2.6.3.
# Now notifies of problem with version of dbm shipped with A/UX
# Last modified 
# Sun Jan  5 11:16:41 WET 1997

case "$cc" in
*gcc*)	optimize='-O2'
	ccflags="$ccflags -D_POSIX_SOURCE"
	echo "Setting hints for GNU CC."
	;;
*)	optimize='-O'
	ccflags="$ccflags -B/usr/lib/big/ -DPARAM_NEEDS_TYPES -D_POSIX_SOURCE"
	POSIX_cflags='ccflags="$ccflags -ZP -Du_long=U32"'
	echo "Setting hints for Apple's CC.  If you plan to use"
	echo "GNU CC, please rerun this Configure script as:"
	echo "./Configure -Dcc=gcc"
	;;
esac
test -r ./broken-db.msg && . ./broken-db.msg
