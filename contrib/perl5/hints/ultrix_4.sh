# hints/ultrix_4.sh
# Last updated by Andy Dougherty  <doughera@lafcol.lafayette.edu>
# Fri Feb 10 10:04:51 EST 1995
#
# Use   Configure -Dcc=gcc   to use gcc.
#
# This used to use -g, but that pulls in -DDEBUGGING by default.
case "$optimize" in
'')
	# recent versions have a working compiler.
	case "$osvers" in
	*4.[45]*)	optimize='-O2' ;;
	*)		optimize='none' ;;
	esac
	;;
esac

# Some users have reported Configure runs *much* faster if you 
# replace all occurences of /bin/sh by /bin/sh5
# Something like:
#   sed 's!/bin/sh!/bin/sh5!g' Configure > Configure.sh5
# Then run "sh5 Configure.sh5 [your options]"

case "$myuname" in
*risc*) cat <<EOF >&4
Note that there is a bug in some versions of NFS on the DECStation that
may cause utime() to work incorrectly.  If so, regression test io/fs
may fail if run under NFS.  Ignore the failure.
EOF
esac

# Compiler flags that depend on osversion:
case "$cc" in
*gcc*) ;;
*)
    case "$osvers" in
    *4.1*)	ccflags="$ccflags -DLANGUAGE_C -Olimit 3800" ;;
    *4.2*)	ccflags="$ccflags -DLANGUAGE_C -Olimit 3800"
		# Prototypes sometimes cause compilation errors in 4.2.
		prototype=undef   
		case "$myuname" in
		*risc*)  d_volatile=undef ;;
		esac
		;;
    *4.3*)	ccflags="$ccflags -std1 -DLANGUAGE_C -Olimit 3800" ;;
    *)	ccflags="$ccflags -std -Olimit 3800" ;;
    esac
    ;;
esac

# Other settings that depend on $osvers:
case "$osvers" in
*4.1*)	;;
*4.2*)	libswanted=`echo $libswanted | sed 's/ malloc / /'` ;;
*4.3*)	;;
*)	ranlib='ranlib' ;;
esac

# Settings that don't depend on $osvers:

util_cflags='ccflags="$ccflags -DLOCALE_ENVIRON_REQUIRED"'
groupstype='int'
# This will cause a WHOA THERE warning, but it's accurate.  The
# configure test should be beefed up to try using the field when
# it can't find any of the standardly-named fields.
d_dirnamlen='define'

# Ultrix can mmap only character devices, not regular files,
# which is rather useless state of things for Perl.
d_mmap='undef'
