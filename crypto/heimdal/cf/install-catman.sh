#!/bin/sh
#
# $Id: install-catman.sh,v 1.3 2001/09/29 16:05:38 assar Exp $
#
# install preformatted manual pages

INSTALL_DATA="$1"; shift
mkinstalldirs="$1"; shift
srcdir="$1"; shift
manbase="$1"; shift
suffix="$1"; shift

for f in "$@"; do
	base=`echo "$f" | sed 's/\(.*\)\.\([^.]*\)$/\1/'`
	section=`echo "$f" | sed 's/\(.*\)\.\([^.]*\)$/\2/'`
	mandir="$manbase/man$section"
	catdir="$manbase/cat$section"
	c="$base.cat$section"

	if test -f "$srcdir/$c"; then
		if test \! -d "$catdir"; then
			eval "$mkinstalldirs $catdir"
		fi
		eval "echo $INSTALL_DATA $srcdir/$c $catdir/$base.$suffix"
		eval "$INSTALL_DATA $srcdir/$c $catdir/$base.$suffix"
	fi
	for link in `sed -n -e '/SYNOPSIS/q;/DESCRIPTION/q;s/^\.Nm \([^ ]*\).*/\1/p' $srcdir/$f`; do
		if [ "$link" != "$base" ]; then
			target="$mandir/$link.$section"
			for cmd in "ln -f $mandir/$base.$section $target" \
				   "ln -s $base.$section $target" \
				   "cp -f $mandir/$base.$section $target"
			do
				if eval "$cmd"; then
					eval echo "$cmd"
					break
				fi
			done
			if test -f "$srcdir/$c"; then
				target="$catdir/$link.$suffix"
				for cmd in "ln -f $catdir/$base.$suffix $target" \
					   "ln -fs $base.$suffix $target" \
					   "cp -f $catdir/$base.$suffix $target"
				do
					if eval "$cmd"; then
						eval echo "$cmd"
						break
					fi
				done
			fi
		fi
	done
done
