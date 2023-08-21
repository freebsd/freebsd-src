#!/bin/sh

if [ ! -f version.sh ] && [ ! -d .git ]
then
	echo Your copy of unifdef is incomplete 1>&2
	exit 1
fi

[ -f version.sh ] && . ./version.sh

if [ -d .git ]
then
	GV=$(git describe | sed 's|-g*|.|g;s|[.]|-|')
	git update-index -q --refresh
	if git diff-index --quiet HEAD
	then
		GD="$(git show --pretty=format:%ai -s HEAD)"
	else
		GD="$(date +'%Y-%m-%d %H:%M:%S %z')"
		GV=$GV.XX
	fi
	[ unifdef -nt unifdef.c ] &&
	[ unifdef -nt unifdef.h ] &&
		GD="$D"
	if [ "$GV $GD" != "$V $D" ]
	then
		echo "version $V $D"   1>&2
		echo "     -> $GV $GD" 1>&2
		V="$GV"
		D="$GD"
		echo "V=\"$V\""  >version.sh
		echo "D=\"$D\"" >>version.sh
		rm -f version.h
	fi
fi

if [ ! -f version.h ]
then
	printf '"@(#) $Version: %s $\\n"\n' "$V" >version.h
	printf '"@(#) $Date: %s $\\n"\n'   "$D" >>version.h
fi
