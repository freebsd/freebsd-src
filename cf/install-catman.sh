#!/bin/sh
#
# $Id$
#
# install preformatted manual pages

cmd="$1"; shift
INSTALL_DATA="$1"; shift
mkinstalldirs="$1"; shift
srcdir="$1"; shift
manbase="$1"; shift
suffix="$1"; shift
catinstall="${INSTALL_CATPAGES-yes}"

for f in "$@"; do
        echo $f
	base=`echo "$f" | sed 's/\.[^.]*$//'`
	section=`echo "$f" | sed 's/^[^.]*\.//'`
	mandir="$manbase/man$section"
	catdir="$manbase/cat$section"
	c="$base.cat$section"

	if test "$catinstall" = yes -a -f "$srcdir/$c"; then
		if test "$cmd" = install ; then
			if test \! -d "$catdir"; then
				eval "$mkinstalldirs $catdir"
			fi
			eval "echo $INSTALL_DATA $srcdir/$c $catdir/$base.$suffix"
			eval "$INSTALL_DATA $srcdir/$c $catdir/$base.$suffix"
		elif test "$cmd" = uninstall ; then
			eval "echo rm -f $catdir/$base.$suffix"
			eval "rm -f $catdir/$base.$suffix"
		fi
	fi
	for link in `sed -n -e '/SYNOPSIS/q;/DESCRIPTION/q;s/^\.Nm \([^ ]*\).*/\1/p' $srcdir/$f`; do
		if test "$link" = "$base" ; then
			continue
		fi
		if test "$cmd" = install ; then
			target="$mandir/$link.$section"
			for lncmd in "ln -f $mandir/$base.$section $target" \
				   "ln -s $base.$section $target" \
				   "cp -f $mandir/$base.$section $target"
			do
				if eval "$lncmd"; then
					eval echo "$lncmd"
					break
				fi
			done
			if test "$catinstall" = yes -a -f "$srcdir/$c"; then
				eval target="$catdir/$link.$suffix"
				eval source="$catdir/$base.$suffix"
				for lncmd in "ln -f $source $target" \
					   "ln -fs $source $target" \
					   "cp -f $catdir/$source $target"
				do
					if eval "$lncmd"; then
						eval echo "$lncmd"
						break
					fi
				done
			fi
		elif test "$cmd" = uninstall ; then
			target="$mandir/$link.$section"
			eval "echo rm -f $target"
			eval "rm -f $target"
			if test "$catinstall" = yes; then
				target="$catdir/$link.$suffix"
				eval "echo rm -f $target"
				eval "rm -f $target"
			fi
		fi
	done
done
