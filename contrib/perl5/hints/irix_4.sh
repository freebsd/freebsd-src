#irix_4.sh
# Last modified Fri May  5 14:06:37 EDT 1995
optimize='-O1'

# Does Configure really get these wrong? Why?
d_voidsig=define
d_charsprf=undef

case "$cc" in
*gcc*) ccflags="$ccflags -D_BSD_TYPES" ;;
*) ccflags="$ccflags -ansiposix -signed" ;;
esac

# This hint due thanks Hershel Walters <walters@smd4d.wes.army.mil>
# Date: Tue, 31 Jan 1995 16:32:53 -0600 (CST)
# Subject: IRIX4.0.4(.5? 5.0?) problems
# I don't know if they affect versions of perl other than 5.000 or
# versions of IRIX other than 4.0.4.
#
cat <<'EOM' >&4
If you have problems, you might have try including
	-DSTANDARD_C -cckr 
in ccflags.
EOM

case "$usethreads" in
$define|true|[yY]*)
        cat >&4 <<EOM
IRIX `uname -r` does not support POSIX threads.
You should upgrade to at least IRIX 6.2 with pthread patches.
EOM
	exit 1
	;;
esac

