#!/bin/sh

#   Copyright (C) 1989 Free Software Foundation, Inc.
#   
#   genclass test program by Wendell C. Baker 

#This file is part of GNU libg++.

#GNU libg++ is free software; you can redistribute it and/or modify
#it under the terms of the GNU General Public License as published by
#the Free Software Foundation; either version 1, or (at your option)
#any later version.

#GNU libg++ is distributed in the hope that it will be useful,
#but WITHOUT ANY WARRANTY; without even the implied warranty of
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#GNU General Public License for more details.

#You should have received a copy of the GNU General Public License
#along with GNU libg++; see the file COPYING.  If not, write to
#the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

#
# test.sh
#
name=gentest.sh
usage="$name" ;
genclass=

case "$1" in
-usage)
    #
    # -usage
    #
    echo "usage: $usage" 1>&2 ;
    exit 0;
    ;;
-version)
    #
    # -version
    #
    version="`expr '$Revision: 1.3 $' : '.*Revision: \(.*\) .*'`" ;
    echo "$name: version $version" ;
    exit 0;
    ;;
-requires)
    #
    # -requires
    #
    echo genclass ;
    exit 0;
    ;;
-genclass)
    #
    shift; genclass=$1
    ;;
esac ;

# pull it in from the environment
[ "$TRACE" = "" ] || set -xv 

if [ "${genclass}" = "" ] 
then 
	genclass="./genclass"
fi

for arg in -usage -version -requires -catalog -list ; do
    echo "---------- genclass $arg ----------"
    ${genclass} $arg
    echo "-----------"
done ;

arg="-catalog PQ Set"
echo "---------- genclass $arg ----------"
${genclass} $arg
echo "-----------"

arg="-list Map Stack"
echo "---------- genclass $arg ----------"
${genclass} $arg
echo "-----------"

std1=int ;
std2=char ;

#
# Do all of them with the single-type syntax
# The Map-based classes are expected to fail (good)
#
for proto in `${genclass} -list` ; do
    file_h=$std1.$proto.h
    file_cc=$std1.$proto.cc
    files="$file_h $file_cc" ;
    echo "Generating: genclass $std1 ref $proto"
    ${genclass} $std1 ref $proto
    if [ $? != 0 ] ; then
	echo "Generation for $std1-$proto failed"
    else
        echo "Checking for badsub"
	egrep '<[TC]&?>' $files
	echo "removing $files"
	rm $files
    fi ;
    echo ""

    nonstd1=fig
    file_h=$nonstd1$proto.h
    file_cc=$nonstd1$proto.cc
    files="$file_h $file_cc" ;
    echo "genclass $std1 ref $proto $nonstd1"
    ${genclass} $std1 ref $proto $nonstd1
    if [ $? != 0 ] ; then
	echo "Generation for $std1-$proto failed"
    else
	echo "Checking for badsub"
	egrep '<[TC]?>' $files
	echo "removing $files"
	rm $files
    fi ;
    echo ""

done ;

#
# Do them all again with the -2 syntax
# None are expected to fail because there is no
# way to tell something that requires the single-type syntax
#
for proto in `${genclass} -list` ; do
    file1_h=$std1.$proto.h
    file1_cc=$std1.$proto.cc
    files1="$file1_h $file1_cc";
    file2_h=$std1.$std2.$proto.h
    file2_cc=$std1.$std2.$proto.cc
    files2="$file2_h $file2_cc" ;
    files="$file1_h $file1_cc $file2_h $file2_cc" ;
    echo "Generating: genclass -2 $std1 ref $std2 val $proto"
    ${genclass} -2 $std1 ref $std2 val $proto
    if [ $? != 0 ] ; then
	echo "Generation for $std1-$std2-$proto failed"
    else
	echo "Checking for badsub"
	if [ -f $file1_h ] ; then
	    # then $file1_cc is expected to exist
	    egrep '<[TC]&?>' $files1
	    echo "removing $files1"
	    rm $files1
	else 
	    # then [ -f $file2_h ]
	    # and $file2_cc is expected to exist
	    egrep '<[TC]&?>' $files2
	    echo "removing $files2"
	    rm $files2
        fi ;
    fi ;
    echo ""

    nonstd=fig
    file_h=$nonstd$proto.h
    file_cc=$nonstd$proto.cc
    files="$file_h $file_cc" ;
    echo "Generating: genclass -2 $std1 ref $std2 val $proto $nonstd"
    ${genclass} -2 $std1 ref $std2 val $proto $nonstd
    if [ $? != 0 ] ; then
	echo "Generation for $std1-$std2-$proto failed"
    else
        echo "Checking for badsub"
	egrep '<[TC]&?>' $files
	echo "removing $files"
	rm $files
    fi ;
    echo ""

done ;
