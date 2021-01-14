#!/bin/sh

# Import bmake

ECHO=
GIT=${GIT:-git}

# For consistency...
Error() {
    echo ERROR: ${1+"$@"} >&2
    exit 1
}

Cd() {
    [ $# -eq 1 ] || Error "Cd() takes a single parameter."
    cd $1 || Error "cannot \"cd $1\" from $PWD"
}

# Call this function and then follow it by any specific import script additions
option_parsing() {
    local _shift=$#
    # Parse command line options
    while :
    do
        case "$1" in
	*=*) eval "$1"; shift;;
	--) shift; break;;
	-a) TARBALL=$2; shift 2;;
	-n) ECHO=echo; shift;;
	-P) PR=$2; shift 2;;
	-r) REVIEWER=$2; shift 2;;
	-u) url=$2; shift 2;;
	-h) echo "Usage:";
	    echo "  "$0 '[-ahnPr] [TARBALL=] [PR=] [REVIEWER=]'
	    echo "  "$0 '-a <filename>	  # (a)rchive'
	    echo "  "$0 '-h			  # print usage'
	    echo "  "$0 '-n			  # do not import, check only.'
	    echo "  "$0 '-P <PR Number>	  # Use PR'
	    echo "  "$0 '-r <reviewer(s) list>  # (r)eviewed by'
	    echo "  "$0 'PR=<PR Number>'
	    echo "  "$0 'REVIEWER=<reviewer(s) list>'
	    exit 1;;
	*) break;;
	esac
    done
    return $(($_shift - $#))
}

###

option_parsing "$@"
shift $?

TF=/tmp/.$USER.$$
Cd `dirname $0`
test -s ${TARBALL:-/dev/null} || Error need TARBALL
here=`pwd`
# thing should match what the TARBALL contains
thing=`basename $here`

case "$thing" in
bmake) (cd .. && tar zxf $TARBALL);;
*) Error "we should be in bmake";;
esac

VERSION=`grep '^_MAKE_VERSION' VERSION | sed 's,.*=[[:space:]]*,,'`

rm -f *~
mkdir -p ../tmp

if [ -z "$ECHO" ]; then
    # new files are handled automatically
    # but we need to rm if needed
    $GIT diff FILES | sed -n '/^-[^-]/s,^-,,p'  > $TF.rm
    test -s $TF.rm && xargs rm -f < $TF.rm
    $GIT add -A
    $GIT diff --staged | tee ../tmp/bmake-import.diff
    echo "$GIT tag -a vendor/NetBSD/bmake/$VERSION" > ../tmp/bmake-post.sh
    echo "After you commit, run $here/../tmp/bmake-post.sh"
else
    # FILES is kept sorted so we can determine what was added and deleted
    $GIT diff FILES | sed -n '/^+[^+]/s,^+,,p'  > $TF.add
    $GIT diff FILES | sed -n '/^-[^-]/s,^-,,p'  > $TF.rm
    test -s $TF.rm && { echo Removing:; cat $TF.rm; }
    test -s $TF.add && { echo Adding:; cat $TF.add; }
    $GIT diff
fi
