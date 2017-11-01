:
# $FreeBSD$

# "global" vars
ECHO=
# Set SVN variables
#  select the local subversion site
SVN=${SVN:-/usr/local/bin/svn}

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

rm -rf $thing
tar zxf $TARBALL

# steps unique to bmake
VERSION=`grep '^_MAKE_VERSION' bmake/VERSION | sed 's,.*=[[:space:]]*,,'`
rm -rf bmake/missing

# the rest should be common
('cd' dist && $SVN list -R) | grep -v '/$' | sort > $TF.old
('cd' $thing && find . -type f ) | cut -c 3- | sort > $TF.new
comm -23 $TF.old $TF.new > $TF.rmlist
comm -13 $TF.old $TF.new > $TF.addlist
[ -s $TF.rmlist ] && { echo rm:; cat $TF.rmlist; }
[ -s $TF.addlist ] && { echo add:; cat $TF.addlist; }
('cd' $thing && tar cf - . | tar xf - -C ../dist)
('cd' dist
test -s $TF.rmlist && xargs $SVN rm < $TF.rmlist
test -s $TF.addlist && xargs $SVN --parents add < $TF.addlist
)

url=`$SVN info | sed -n '/^URL:/s,URL: ,,p'`
echo "After committing dist... run; sh ./tag.sh"
echo "$SVN cp -m 'tag $thing-$VERSION' $url/dist $url/$VERSION" > tag.sh

