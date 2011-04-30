#!/bin/sh
# $Id: make-tar.sh,v 1.6 2010/11/06 19:59:07 tom Exp $
##############################################################################
# Copyright (c) 2010 Free Software Foundation, Inc.                          #
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
# Construct a tar-file containing only the Ada95 tree as well as its associated
# documentation.  The reason for doing that is to simplify distributing the
# ada binding as a separate package.

TARGET=`pwd`

: ${ROOTNAME:=ncurses-Ada95}
: ${DESTDIR:=$TARGET}
: ${TMPDIR:=/tmp}

# This can be run from either the Ada95 subdirectory, or from the top-level
# source directory.  We will put the tar file in the original directory.
test -d ./Ada95 && cd ./Ada95

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

# Add the ada documentation.
mkdir $BUILD/$ROOTNAME/doc || exit
cd ../doc/html || exit

cp -p -r Ada* $BUILD/$ROOTNAME/doc/
cp -p -r ada $BUILD/$ROOTNAME/doc/

cd $BUILD || exit 

# There is no need for this script in the tar file.
rm -f $ROOTNAME/make-tar.sh

# Remove build-artifacts.
find . -name RCS -exec rm -rf {} \;
find . -name "*.gz" -exec rm -rf {} \;

# Make the files writable...
chmod -R u+w .

tar cf - $ROOTNAME | gzip >$DESTDIR/$ROOTNAME.tar.gz
cd $DESTDIR

pwd
ls -l $ROOTNAME.tar.gz
