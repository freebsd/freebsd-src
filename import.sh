#!/bin/sh
# $FreeBSD$
#
# Import script for libxo.  Typically invoked as:
#
#    cd work/bsd/base
#    sh ./vendor/Juniper/libxo/import.sh -v 0.4.6
#
# Add "-n" to avoid svn actions
# Add "-d" to generate docs
#
# Phil Shafer (phil@), April 2016
#

PROJECT=libxo

BUILDWORLD="make -j8 buildworld -DWITH_META_MODE -DNO_CLEAN -DWITHOUT_TESTS"

#"global" vars
# Set SVN variables
#  select the local subversion site
SVN=${SVN:-/usr/local/bin/svn}
# "Real" SVN, even if "-n"
RSVN=$SVN
GMAKE=${GMAKE:-gmake}

# For consistency...
Error() {
	echo ERROR: ${1+"$@"} >&2
	exit 1
}

Cd() {
	[ $# -eq 1 ] || Error "Cd() takes a single parameter."
	cd $1 || Error "cannot \"cd $1\" from $PWD"
        info "Directory =" `pwd`
}

siginfo() {
    if [ ! -z "$CMD" ]; then
        info "CMD is $CMD"
    fi
}

trap 'siginfo' SIGINFO
trap 'siginfo' SIGCONT

run() {
    desc="$1"
    cmd="$2"
    CMD="$2"

    if [ "$DOC" = doc ]; then
        echo " == $desc"
        echo "     - $cmd"
        echo " "
    else
        echo "===="
        echo "Phase: $desc"
        echo "  Run: $cmd"
        okay
        # We need to eval to handle "&&" in commands
        eval $cmd
        okay
    fi
}

info() {
    echo " -- " "$@"
}

okay() {
    if [ -z "$OKAY" ]; then
        /bin/echo -n "proceed? "
        read okay
        case "$okay" in
        [QqNn]*) echo "exiting"; exit 1;;
        esac
    fi
}

spew_words () {
    for i in "$@"; do
        echo $i
    done
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
                -d) DOC=doc; shift;;
                -D) VENDOR_DIR=$2; shift 2;;
                -f) FETCH=yes; shift;;
		-n) SVN='echo svn'; shift;;
                -p) PROJECT=$2; shift 2;;
		-P) PR=$2; shift 2;;
		-r) REVIEWER=$2; shift 2;;
		-u) UPDATE=no; shift;;
                -v) VERS=$2; shift 2;;
                -y) OKAY=yes; shift;;

		-h) echo "Usage:";
     echo "  "$0 '[-ahnPr] [TARBALL=] [PR=] [REVIEWER=]'
			echo "  -a <filename>      -- name or tarball"
			echo "  -d                 -- generate documentation"
			echo "  -f                 -- force fetch of tarballs"
			echo "  -h                 -- print usage"
			echo "  -n                 -- do not import, check only"
			echo "  -v <version>       -- version to import"
			echo "  -y                 -- answer 'yes'"
			exit 1;;
		*) break;;
		esac
	done
	return $(($_shift - $#))
}

###

option_parsing "$@"
shift $?

Cd `dirname $0`
CWD=`pwd`

TOP=`echo $CWD | sed 's:/vendor/.*::'`
info "TOP = $TOP"

Cd $TOP
HEAD=$TOP/head
info "HEAD = $HEAD"

mkdir -p ../obj
MAKEOBJDIRPREFIX=`cd ../obj; pwd`
export MAKEOBJDIRPREFIX

if [ -z "$VENDOR_DIR" ]; then
    VENDOR_DIR=`echo $CWD | sed 's:.*/vendor/::'`
fi

#--------------------------------------------------------------

info "CWD = $CWD"
info "VENDOR_DIR = $VENDOR_DIR"
info "VERS = $VERS"
info "BUILDWORLD = $BUILDWORLD"
DATESTAMP=`date "+%Y-%m-%d-%H-%M"`

[ -z "$VERS" ] && Error "missing version argument (-v)"

run "show any local changes" "diff -rbu $CWD/dist $HEAD/contrib/libxo"

Cd $HEAD
run "updating all" "svn update"

if [ ! -z "$UPDATE" ]; then
    run "building the entire world" "script $MAKEOBJDIRPREFIX/out.$DATESTAMP.before $BUILDWORLD"
fi

Cd $CWD
mkdir -p ~/tars

# We use the source tarball from git since it has no frills
# (and libxo has a lot of frills.....)
URL=https://github.com/Juniper/libxo/archive/$VERS.tar.gz
BASEURL=libxo-`basename $URL`
TARBALL=~/tars/$BASEURL

info "BASEURL = $BASEURL"

if [ "$FETCH" = "yes" -o ! -f $TARBALL ]; then
    run "fetching tarball ($URL)" "fetch -o $TARBALL $URL"
    [ -s $TARBALL ] || Error "fetch failed to get file"
fi

# We need the release tar ball for the HTML docs, nothing more
DOCURL=https://github.com/Juniper/libxo/releases/download/$VERS/libxo-$VERS.tar.gz
DOCBALL=~/tars/doc-$BASEURL

if [ "$FETCH" = "yes" -o ! -f $DOCBALL ]; then
    run "fetching doc tarball" "fetch -o $DOCBALL $DOCURL"
    test -s ${DOCBALL} || Error need DOCBALL
fi

# BASE should match what the TARBALL contains
BASE=`basename $TARBALL .tar.gz`
VERSION=`echo $BASE | sed 's/libxo-//'`

TF=$BASE/info

run "untarring source files TARBALL" "tar zxf $TARBALL"
run "untarring html manual" \
    "tar zxf $DOCBALL libxo-$VERS/doc/libxo-manual.html"

# List of top-level files we want to ignore
TOPJUNKFILES="\
libxo-* \
tag.sh"

# List of files in the tarball that we want to ignore
DISTJUNKFILES="\
Makefile.in \
aclocal.m4 \
ar-lib \
autom4te.cache \
bin* \
build* \
compile \
configure \
config.guess \
config.sub \
depcomp \
doc/Makefile.in \
info* \
install-sh \
ltmain.sh \
m4 \
missing \
patches*"

# List of directories that need a "Makefile.in" ignored
IGNOREDIRS="\
doc \
encoder \
encoder/cbor \
encoder/test \
libxo \
tests \
tests/core \
tests/gettext \
tests/xo \
xo \
xohtml \
xolint \
xopo"

SEDNUKE="sed -e '/^bin/d' -e '/^build/d' -e '/^info/d' -e '/^patches/d'"

run "writing .svnignore" \
    "(for i in $TOPJUNKFILES; do echo \$i ; done ) > .svnignore"
run "setting svn:ignore for ." \
    "$SVN propset svn:ignore -F .svnignore ."

run "writing dist/.svnignore" \
    "(for i in $DISTJUNKFILES; do echo \$i ; done ) > dist/.svnignore"
run "setting svn:ignore for dist" \
    "(cd dist && $SVN propset svn:ignore -F .svnignore .)"
run "setting svn:ignore for Makefile.in dirs" \
    "for dir in $IGNOREDIRS; do (cd dist/\$dir && $SVN propset svn:ignore Makefile.in .) ; done"

# the rest should be common
run "making list of files in existing tree" \
    "(cd dist && $RSVN list -R) | grep -v '/$' | sort > $TF.old"

run "making list of files in incoming tree" \
    "(echo 'x .svnignore' ; cd $BASE && find . -type f ) | cut -c 3- | $SEDNUKE | sort > $TF.new"

run "making list of deleted files" "comm -23 $TF.old $TF.new | tee $TF.rmlist"
run "making list of new files" "comm -13 $TF.old $TF.new | tee $TF.addlist"

run "copying contents over to dist/" "(cd $BASE && tar cf - . | tar xf - -C ../dist)"
run "removing old files from svn" \
    "(test -s $TF.rmlist && cd dist && xargs $SVN rm < ../$TF.rmlist)"
run "adding new files to svn" \
    "(test -s $TF.addlist && cd dist && xargs $SVN --parents add < ../$TF.addlist )"

url=`$RSVN info | sed -n '/^URL:/s,URL: ,,p'`
info "URL = $url"

run "building tag script" "(echo set -x ; echo $SVN cp $url/dist $url/$VERSION ) > tag.sh"

Cd $CWD/dist
run "autoreconf" "autoreconf --install --force"

Cd $CWD/dist/build
run "configure for testing" "../configure --prefix $CWD/dist/build/root"
run "build and test" \
        "${GMAKE} clean && ${GMAKE} && ${GMAKE} install && ${GMAKE} test"

# Freebsd lacks stock gettext, so don't build it
run "configure for real" "../configure --disable-gettext --prefix /usr"
run "build for real" \
        "${GMAKE} clean && ${GMAKE}"


# Move over and build the source tree
Cd $HEAD

run "copying xo_config.h" "(echo '/* \$FreeBSD\$ */' ; cat $CWD/dist/build/libxo/xo_config.h ) > $HEAD/lib/libxo/xo_config.h"
run "copying add.man" "(echo '.\\\" \$FreeBSD\$' ; cat $CWD/dist/build/libxo/add.man ) > $HEAD/lib/libxo/add.man"

#BUILDDIRS="lib/libxo usr.bin/xo"
#for dir in $BUILDDIRS ; do
    #Cd $HEAD/$dir
    #run "making build dir '$dir'" "make LIBXOSRC=$CWD/dist"
#done

run "building the entire world" "script $MAKEOBJDIRPREFIX/out.$DATESTAMP $BUILDWORLD LIBXOSRC=$CWD/dist"

# Okay, so now it all builds!!  Now we can start committing....

Cd $CWD

run "show svn stat for 'dist'" "$SVN stat"
run "show svn diff for 'dist'" "$SVN diff --no-diff-deleted"

Cd $HEAD

run "show svn stat for 'head'" "$SVN stat"
run "show svn diff for 'head'" "$SVN diff --no-diff-deleted"

# Start committing

Cd $CWD
run "commit changes to 'dist'" "$SVN -m 'Import $PROJECT $VERSION' commit dist"
run "show svn stat for 'dist'" "$SVN stat dist"
run "show svn diff for 'dist'" "$SVN diff dist --no-diff-deleted"


run "tagging repo" "$SVN cp -m 'Tag $PROJECT $VERSION' $url/dist $url/$VERSION"
run "refresh libxo" "$SVN update"

Cd $HEAD/contrib/$PROJECT
CONTRIB=`pwd`

run "copy dist to contrib" "$SVN merge --accept=postpone svn+ssh://repo.freebsd.org/base/vendor/Juniper/libxo/dist ."

run "show svn stat for 'head'" "$SVN stat"
run "checking merge issues" "$SVN diff --no-diff-deleted --old=svn+ssh://repo.freebsd.org/base/vendor/Juniper/libxo/dist --new=."

#run "committing to contrib" "$SVN commit -m 'Import $PROJECT $VERSION'"

Cd $HEAD
run "show svn stat for 'head'" "$SVN stat"
run "show svn diff for 'head'" "$SVN diff --no-diff-deleted"
run "commit changes to 'head'" "$SVN commit"
run "show svn stat for 'head'" "$SVN stat"

exit 0
