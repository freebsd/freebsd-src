:
# $FreeBSD$

# "global" vars
ECHO=
# Set SVN variables
#  select the local subversion site
SVN=${SVN:-/usr/local/bin/svn}
SITE=${SITE:-ftp://ftp.netbsd.org/pub/NetBSD/misc/sjg}


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
		-b) TARBALL=$2; shift 2;;
		-n) ECHO=echo; shift;;
		-P) PR=$2; shift 2;;
		-r) REVIEWER=$2; shift 2;;
		-u) url=$2; shift 2;;
		-v) VERSION=$2; shift 2;;
		-h) echo "Usage:";
     echo "  "$0 '[-abhnPrv] [ARCHIVE=] [TARBALL=] [PR=] [REVIEWER=] [VERSION=]'
			echo "  "$0 '-a <filename>	  # (a)rchive'
			echo "  "$0 '-b <filename>	  # tar(b)all'
			echo "  "$0 '-h			  # print usage'
			echo "  "$0 '-n			  # do not import, check only.'
			echo "  "$0 '-P <PR Number>	  # Use PR'
			echo "  "$0 '-r <reviewer(s) list>  # (r)eviewed by'
			echo "  "$0 '-v <version "number">  # (v)ersion#'
			echo "  "$0 'PR=<PR Number>'
			echo "  "$0 'REVIEWER=<reviewer(s) list>'
			echo "  "$0 'VERSION=<version "number">'
			exit 1;;
		*) break;;
		esac
	done
	TARBALL=${ARCHIVE:-${TARBALL}}
	return $(($_shift - $#))
}

# Call this function after all argument parsing has been done.
sanity_checks() {
    # Do we have a working Subversion client?
    ${SVN} --version -q >/dev/null || \
	Error "Cannot find a working subversion client."

    # Verify that a PR number and reviewer(s) were specified on the
    # command line.
    [ "$VERSION" ] || Error "We will a version \"number\" (can be a string).  Use VERSION=<version>."
    # Need one (and only one) of ${url} or ${TARBALL} set.
    [ "${url:-$TARBALL}" -a "${url:-$TARBALL}" != "${TARBALL:-$url}" ] && Error "Please set either \"url\" or \"TARBALL\" (not both) in your import script."
    [ -d dist ] || Error "The directory dist/ does not exist."
}

###

option_parsing "$@"
shift $?
sanity_checks

fetch $SITE/bmake-${VERSION}.tar.gz.sha1
fetch $SITE/bmake-${VERSION}.tar.gz

HAVE=`sha1 bmake-${VERSION}.tar.gz`
WANT=`cat bmake-${VERSION}.tar.gz.sha1`
if [ x"$HAVE" != x"$WANT" ]; then
	Error "Fetched distfile does not have the expected SHA1 hash."
fi

tar xf bmake-${VERSION}.tar.gz
rm -rf bmake/missing

svn-vendorimport.sh bmake dist
${SVN} stat dist

rm -f bmake-${VERSION}.tar.gz bmake-${VERSION}.tar.gz.sha1

echo "Import the ${VERSION} release of the \"Portable\" BSD make tool (from NetBSD).

Submitted by:	Simon Gerraty <sjg@juniper.net>" > /tmp/commit-log

${ECHO} ${SVN} ci -F /tmp/commit-log dist

SVNURL=$(${SVN} info | grep URL: | awk '{print $2}')

${ECHO} ${SVN} copy \
    -m "\"Tag\" the ${VERSION} Portable BSD make import." \
    ${SVNURL}/dist ${SVNURL}/${VERSION}
