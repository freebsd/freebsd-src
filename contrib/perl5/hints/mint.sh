# hints/mint.sh
#
# talk to gufl0000@stud.uni-sb.de if you want to change this file.
# Please read the README.mint file.
#
# misc stuff

case `uname -m` in
	atarist*) archname="m68000-mint"
		  ;;
	*)	  archname="m68k-mint"
		  ;;
esac		  

here=`pwd | tr -d '\015'`

cc='gcc'

# The weird include path is really to work around some bugs in
# broken system header files.
ccflags="-D__MINT__ -Uatarist -DDEBUGGING -I$here/../mint"

# libs

libpth="$prefix/lib /usr/lib /usr/local/lib"
glibpth="$libpth"
xlibpth="$libpth"

libswanted='gdbm socket port m'
so='none'

#
# compiler & linker flags
#
optimize='-O2 -fomit-frame-pointer -fno-defer-pop -fstrength-reduce'

# The setlocale function in the MiNTLib is actually a bad joke.  We 
# lend a workaround from Ultrix.  If neither LC_ALL nor LANG is
# set in the environment, perl won't complain.  If one is set to
# anything but "C" you will see a warning.  Note that you can
# still use the GNU extension "$LANGUAGE" if you want to use
# the i18n features of some GNU packages.
util_cflags='ccflags="$ccflags -DLOCALE_ENVIRON_REQUIRED"'

#
# Some good answers to the questions in Configure:
usenm='true'
d_suidsafe='true'
clocktype='long'
usevfork='true'
d_fsetpos='fpos_t'
gidtype='gid_t'
groupstype='gid_t'
lseektype='long'
models='none'
modetype='mode_t'
sizetype='size_t'
timetype='time_t'
uidtype='uid_t'

# Don't remove that leading tab character (Configure Black Magic (TM)).
    broken_pwd=
case "`/bin/pwd|tr -d xy|tr '\015\012' 'xy'`" in
*xy) broken_pwd=yes ;;
esac

if test X"$broken_pwd" = Xyes
then
    echo " "
    echo "*** Building fixed 'pwd'... (as described in README.mint) ***"
    echo " "
    cd mint
    make pwd
    cd ..
    if test -x mint/pwd -a -w /usr/bin
    then
	echo " "
	echo "*** Installing fixed 'pwd'... ***"
	echo " "
	cd mint
	make install
	cd ..
	if cmp -s mint/pwd /usr/bin/pwd
	then
	    echo "*** Installed fixed 'pwd' successfully. ***"
	else
	    echo "*** Failed to install fixed 'pwd'.  Aborting. ***"
	    exit 1
	fi
    else
	echo "*** Cannot install fixed 'pwd'.  Aborting. ***"
	exit 1
    fi
fi
