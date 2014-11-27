#!/bin/sh
#
# Simple script for enumerating installed files for a list of directories
#
# usage: add-optional-obsolete-files-entries.sh directory ..
#
# $FreeBSD$

# NO_ROOT doesn't seem to work at a non-installworld, etc level yet
if [ $(id -u) -ne 0 ]; then
	echo "${0##*/}: ERROR: this script must be run as root"
	exit 1
fi

: ${TMPDIR=/tmp}

DESTDIR=$(mktemp -d $TMPDIR/tmp.XXXXXX) || exit
trap "rm -Rf $DESTDIR" EXIT INT TERM

# Don't pollute the output with 
: ${SRCCONF=/dev/null}
: ${__MAKE_CONF=/dev/null}

if [ $# -gt 0 ]
then
	directories=$*
else
	directories=.
fi

export __MAKE_CONF DESTDIR SRCCONF

SRCTOP=$(cd $(make -V'${.MAKE.MAKEFILES:M*/share/mk/sys.mk:H:H:H}'); pwd)

# Don't install the manpage symlinks
(cd $SRCTOP; make hier INSTALL_SYMLINK=true MK_MAN=no >/dev/null)

for directory in $directories
do
	(cd $directory && make install >/dev/null) || exit
done
# Prune empty directories
# XXX: is [ -n ... ] call necessary?
while empty_dirs=$(find $DESTDIR -type d -and -empty) && [ -n "$empty_dirs" ]
do
	rmdir $empty_dirs
done

# Enumerate all of the installed files/directories
(cd $DESTDIR;
 find -s . -type f -mindepth 1 | \
    sed -e 's,^,OLD_FILES+=,' \
        -e '/lib\/.*\.so\.[0-9]\.*/s/OLD_FILES+=/OLD_LIBS+=/g';
 find -d -s . -type d -mindepth 1 -and \! -empty | \
    egrep -v '^\./(boot|s*bin|libexec|usr|usr/include|usr/lib(data)?|usr/libdata/pkgconfig|usr/lib/private|usr/libexec|usr/s*bin|usr/share|usr/share/(examples|man)|usr/share/man/man[0-9])$' | \
    sed -e 's,^,OLD_DIRS+=,'
) | sed -e 's,+=\./,+=,'
