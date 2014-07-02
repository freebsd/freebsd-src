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

###

option_parsing "$@"
shift $?

Cd `dirname $0`
test -s ${TARBALL:-/dev/null} || Error need TARBALL
rm -rf bmake
TF=/tmp/.$USER.$$

tar zxf $TARBALL
MAKE_VERSION=`grep '^MAKE_VERSION' bmake/Makefile | sed 's,.*=[[:space:]]*,,'`
rm -rf bmake/missing
('cd' dist && $SVN list -R) | grep -v '/$' | sort > $TF.old
('cd' bmake && find . -type f ) | cut -c 3- | sort > $TF.new
comm -23 $TF.old $TF.new > $TF.rmlist
comm -13 $TF.old $TF.new > $TF.addlist
[ -s $TF.rmlist ] && { echo rm:; cat $TF.rmlist; }
[ -s $TF.addlist ] && { echo add:; cat $TF.addlist; }
('cd' bmake && tar cf - . | tar xf - -C ../dist)
('cd' dist
test -s $TF.rmlist && xargs $SVN rm < $TF.rmlist
test -s $TF.addlist && xargs $SVN --parents add < $TF.addlist
)

url=`$SVN info | sed -n '/^URL:/s,URL: ,,p'`
echo After committing dist...
echo $SVN cp $url/dist $url/$MAKE_VERSION
