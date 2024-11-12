#!/bin/sh
#
# Copyright (c) 2014 Lev Serebryakov.
# Copyright (c) 2009 Poul-Henning Kamp.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
#
# Usage:
# 	$0 [-cv] PACKAGE_DUMP NANO_PACKAGE_DIR /usr/ports/foo/bar [package.pkg]...
#
# Will symlink/copy the packages listed, including their runtime dependencies,
# from the PACKAGE_DUMP to the NANO_PACKAGE_DIR.
#

: ${PORTSDIR:=/usr/ports}

usage () {
	echo "Usage: $0 [-cv] package-dump-dir nano-package-dir port-dir-or-pkg ..." 1>&2
	exit 2
}

msg () {
	local l
	l=$1 ; shift
	[ "$l" -le "$VERBOSE" ] && echo $*
}

ports_recurse() (
	local outputfile dumpdir type fullpath pkgname p
	outputfile=$1 ; shift
	dumpdir=$1    ; shift
	for p do
		if [ -d "$p" -a -f "$p/Makefile" ] ; then
			msg 3 "$p: full path to port"
			pkgname=`cd "$p" && make package-name`
			type=port
			fullpath=$p
		elif [ -d "${PORTSDIR}/$p" -a -f "${PORTSDIR}/$p/Makefile" ] ; then
			msg 3 "$p: path to port relative to ${PORTSDIR}}"
			pkgname=`cd "${PORTSDIR}/$p" && make package-name`
			type=port
			fullpath=${PORTSDIR}/$p
		elif [ "${p%.pkg}" != "$p" -a -f "$p" ] && pkg info -F "$p" > /dev/null 2>&1 ; then
			msg 3 "$p: full package file name"
			pkgname=`basename "$p" | sed 's/\.pkg$//I'`
			type=pkg
			fullpath=$p
		elif [ "${p%.pkg}" != "$p" -a -f "$dumpdir/$p" ] && pkg info -F "$dumpdir/$p" > /dev/null 2>&1 ; then
			msg 3 "$p: package file name relative to $dumpdir"
			pkgname=`basename "$p" | sed 's/\.pkg$//I'`
			type=pkg
			fullpath=$dumpdir/$p
		elif [ -f "$dumpdir/$p.pkg" ] && pkg info -F "$dumpdir/$p.pkg" > /dev/null 2>&1 ; then
			msg 3 "$p: package name relative to $dumpdir"
			pkgname=`basename "$p"`
			type=pkg
			fullpath=$dumpdir/$p.pkg
		else
			echo "Missing port or package $p" 1>&2
			exit 2
		fi
		if grep -q "^$pkgname\$" "$outputfile" ; then
			msg 3 "$pkgname was added already"
			true
		elif [ "$type" = "port" ] ; then
			(
				cd "$fullpath"
				rd=`make -V RUN_DEPENDS ${PORTS_OPTS}`
				ld=`make -V LIB_DEPENDS ${PORTS_OPTS}`

				for dep in $rd $ld ; do
					arg=`echo $dep | sed 's/^[^:]*:\([^:]*\).*$/\1/'`
					msg 2 "Check $arg as requirement for port $pkgname"
					ports_recurse "$outputfile" "$dumpdir" "$arg"
				done
			)
			msg 1 "Add $pkgname"
			echo "$pkgname" >> "$outputfile"
		else
			dir=`dirname "$p"` # Get directory from SPECIFIED path, not from full path
			if [ "$dir" = "." ] ; then
			  dir=""
			else
			  dir=${dir}/
			fi
			deps=`pkg info -dF "$fullpath" | grep -v "$pkgname:"`
			for dep in $deps ; do
				arg=`echo $dep | sed -e "s|^|$dir|" -e 's/$/.pkg/'`
				msg 2 "Check $arg as requirement for package $pkgname"
				ports_recurse "$outputfile" "$dumpdir" "$arg"
			done
			msg 1 "Add $pkgname"
			echo "$pkgname" >> "$outputfile"
		fi
	done
)

COPY="ln -s"
VERBOSE=0

while getopts cv opt ; do
	case "$opt" in
	  c) COPY="cp -p"              ;;
	  v) VERBOSE=$(($VERBOSE + 1)) ;;
	[?]) usage                     ;;
	esac
done
shift $(( ${OPTIND} - 1 ))

if [ "$#" -lt 3 ] ; then
	usage
fi

NANO_PKG_DUMP=`realpath $1`
shift;
if [ ! -d "$NANO_PKG_DUMP" ] ; then
	echo "$NANO_PKG_DUMP is not a directory" 1>&2
	usage
fi

NANO_PKG_DIR=`realpath $1`
shift;
if [ ! -d "$NANO_PKG_DIR" ] ; then
	echo "$NANO_PKG_DIR is not a directory" 1>&2
	usage
fi

# Cleanup
rm -rf "$NANO_PKG_DIR/"*

PL=$NANO_PKG_DIR/_list
true > "$PL"

for p do
	ports_recurse "$PL" "$NANO_PKG_DUMP" "$p"
done

for i in `cat "$PL"` ; do
	if [ -f "$NANO_PKG_DUMP/$i.pkg" ] ; then
		$COPY "$NANO_PKG_DUMP/$i.pkg" "$NANO_PKG_DIR"
	else
		echo "Package $i misssing in $NANO_PKG_DUMP" 1>&2
		exit 1
	fi
done

rm -f "$PL"
exit 0
