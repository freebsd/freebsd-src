#!/bin/sh
# $Id: make-tar.sh,v 1.10 2011/03/26 20:46:51 tom Exp $
##############################################################################
# Copyright (c) 2010,2011 Free Software Foundation, Inc.                     #
#                                                                            #
# Permission is hereby granted, free of charge, to any person obtaining a    #
# copy of this software and associated documentation files (the "Software"), #
# to deal in the Software without restriction, including without limitation  #
# the rights to use, copy, modify, merge, publish, distribute, distribute    #
# with modifications, sublicense, and/or sell copies of the Software, and to #
# permit persons to whom the Software is furnished to do so, subject to the  #
# following conditions:                                                      #
#                                                                            #
# The above copyright notice and this permission notice shall be included in #
# all copies or substantial portions of the Software.                        #
#                                                                            #
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR #
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   #
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    #
# THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER      #
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    #
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        #
# DEALINGS IN THE SOFTWARE.                                                  #
#                                                                            #
# Except as contained in this notice, the name(s) of the above copyright     #
# holders shall not be used in advertising or otherwise to promote the sale, #
# use or other dealings in this Software without prior written               #
# authorization.                                                             #
##############################################################################
# Construct a tar-file containing only the test tree as well as its associated
# scripts.  The reason for doing that is to simplify distributing the test
# programs as a separate package.

CDPATH=:
export CDPATH

TARGET=`pwd`

: ${PKG_NAME:=ncurses-examples}
: ${ROOTNAME:=ncurses-test}
: ${DESTDIR:=$TARGET}
: ${TMPDIR:=/tmp}

grep_assign() {
	grep_assign=`egrep "^$2\>" "$1" | sed -e "s/^$2[ 	]*=[ 	]*//" -e 's/"//g'`
	eval $2=\"$grep_assign\"
}

grep_patchdate() {
	grep_assign ../dist.mk NCURSES_MAJOR
	grep_assign ../dist.mk NCURSES_MINOR
	grep_assign ../dist.mk NCURSES_PATCH
}

# The rpm spec-file in the ncurses tree is a template.  Fill in the version
# information from dist.mk
edit_specfile() {
	sed \
		-e "s/\\<MAJOR\\>/$NCURSES_MAJOR/g" \
		-e "s/\\<MINOR\\>/$NCURSES_MINOR/g" \
		-e "s/\\<YYYYMMDD\\>/$NCURSES_PATCH/g" $1 >$1.new
	chmod u+w $1
	mv $1.new $1
}

make_changelog() {
	test -f $1 && chmod u+w $1
	cat >$1 <<EOF
`echo $PKG_NAME|tr '[A-Z]' '[a-z]'` ($NCURSES_PATCH) unstable; urgency=low

  * snapshot of ncurses subpackage for $PKG_NAME.

 -- `head -1 $HOME/.signature`  `date -R`
EOF
}

# This can be run from either the subdirectory, or from the top-level
# source directory.  We will put the tar file in the original directory.
test -d ./test && cd ./test
SOURCE=`cd ..;pwd`

BUILD=$TMPDIR/make-tar$$
trap "cd /; rm -rf $BUILD; exit 0" 0 1 2 5 15

umask 077
if ! ( mkdir $BUILD )
then
	echo "? cannot make build directory $BUILD"
fi

umask 022
mkdir $BUILD/$ROOTNAME

cp -p -r * $BUILD/$ROOTNAME/ || exit

# Add the config.* utility scripts from the top-level directory.
for i in . ..
do
	for j in config.guess config.sub install-sh tar-copy.sh
	do
		test -f $i/$j && cp -p $i/$j $BUILD/$ROOTNAME/
	done
done

# Make rpm and dpkg scripts for test-builds
grep_patchdate
for spec in $BUILD/$ROOTNAME/package/*.spec
do
	edit_specfile $spec
done
make_changelog $BUILD/$ROOTNAME/package/debian/changelog

cp -p $SOURCE/NEWS $BUILD/$ROOTNAME

# cleanup empty directories (an artifact of ncurses source archives)

touch $BUILD/$ROOTNAME/MANIFEST 
( cd $BUILD/$ROOTNAME && find . -type f -print |$SOURCE/misc/csort >MANIFEST )

cd $BUILD || exit 

# Remove build-artifacts.
find . -name RCS -exec rm -rf {} \;
find $BUILD/$ROOTNAME -type d -exec rmdir {} \; 2>/dev/null
find $BUILD/$ROOTNAME -type d -exec rmdir {} \; 2>/dev/null
find $BUILD/$ROOTNAME -type d -exec rmdir {} \; 2>/dev/null

# There is no need for this script in the tar file.
rm -f $ROOTNAME/make-tar.sh

# Remove build-artifacts.
find . -name "*.gz" -exec rm -rf {} \;

# Make the files writable...
chmod -R u+w .

tar cf - $ROOTNAME | gzip >$DESTDIR/$ROOTNAME.tar.gz
cd $DESTDIR

pwd
ls -l $ROOTNAME.tar.gz

# vi:ts=4 sw=4
