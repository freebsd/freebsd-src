#!/bin/sh
# $Id: make_sed.sh,v 1.5 1998/02/11 12:13:48 tom Exp $
##############################################################################
# Copyright (c) 1998 Free Software Foundation, Inc.                          #
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
#
# Author: Thomas E. Dickey <dickey@clark.net> 1997
#
# Construct a sed-script to perform renaming within man-pages.  Originally
# written in much simpler form, this one accounts for the common cases of
# section-names in man-pages.

if test $# != 1 ; then
	echo '? expected a single filename'
	exit 1
fi

COL=col$$
INPUT=input$$
UPPER=upper$$
SCRIPT=script$$
RESULT=result$$
rm -f $UPPER $SCRIPT $RESULT
trap "rm -f $COL.* $INPUT $UPPER $SCRIPT $RESULT" 0 1 2 5 15
fgrep -v \# $1 | \
sed	-e 's/[	]\+/	/g' >$INPUT

for F in 1 2 3 4
do
sed	-e 's/\./	/g' $INPUT | \
cut	-f $F > $COL.$F
done
for F in 2 4
do
	tr abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ <$COL.$F >$UPPER
	mv $UPPER $COL.$F 
done
paste $COL.* | \
sed	-e 's/^/s\/\\</' \
	-e 's/$/\//' >$UPPER

# Do the TH lines
sed	-e 's/\//\/TH /' \
	-e 's/	/ /' \
	-e 's/	/ ""\/TH /' \
	-e 's/	/ /' \
	-e 's/\/$/ ""\//' \
	$UPPER >>$RESULT

# Do the embedded references
sed	-e 's/</<fB/' \
	-e 's/	/\\\\fR(/' \
	-e 's/	/)\/fB/' \
	-e 's/	/\\\\fR(/' \
	-e 's/\/$/)\//' \
	$UPPER >>$RESULT

# Finally, send the result to standard output
cat $RESULT
