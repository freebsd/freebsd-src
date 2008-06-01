#!/bin/sh

#   Copyright (C) 1989 Free Software Foundation, Inc.
#   
#   genclass program enhanced by Wendell C. Baker 
#   (original by Doug Lea (dl@rocky.oswego.edu))

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
# genclass -list [proto ...]
# genclass -catalog [proto ...]
# genclass type1 {ref|val} proto [out_prefix]
# genclass -2 type1 {ref|val} type2 {ref, val} proto [out_prefix]
#
# Generate classes from prototypes
#
name=genclass ;
usage="
    $name -list [proto ...]
    $name -catalog [proto ...]
    $name type1 {ref|val} proto [out_prefix]
    $name -2 type1 {ref|val} type2 {ref|val} proto [out_prefix]" ;

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
    # <VERSION> is substituted by the build process.
    # We currently use the libg++ version number (extracted from ../Makefile).
    echo "$name: version <VERSION>" ;
    exit 0;
    ;;
-requires)
    #
    # -requires
    #
    # The following line should contain any nonstandard programs
    # which must be in the users's path (i.e. not referenced by a
    # fullpath);it allows one to check a script for dependencies
    # without exhaustively testing its usages.
    # ... in this case genclass depends on nothing else.
    echo ;
    exit 0;
    ;;
esac ;

# pull it in from the environment
[ "$TRACE" = "" ] || set -xv 

# Search in standard g++ prototype directory and in the current directory
# NOTE: this variable is edited by the install process
PROTODIR=/projects/gnu-cygnus/gnu-cygnus-2/mips/lib/g++-include/gen

pwd=`pwd`

case "$1" in
-catalog*|-list*)
    #
    # genclass -catalog [proto ...]
    # genclass -list [proto ...]
    #
    option="$1" ;
    shift ;

    case $# in
    0)
        #
        # -catalog
        # -list
        #
        select=all ;
        select_pattern=p ;
        ;;
    *)
        #
        # -catalog proto ...
        # -list proto ...
        #
        select="$@" ;
        select_pattern= ;
        for i in $@ ; do
            select_pattern="\
$select_pattern
/.*$i\$/ p
" ;
        done ;

        ;;
    esac ;

    #
    # select_pattern is now a (possibly-vacuous) newline-
    # separated list of patterns of the form:
    #
    #     /.*Proto1$/ p
    #     /.*Proto2$/ p
    #     /.*Proto3$/ p
    #
    # or select_pattern is simply ``p'' to select everything

    # Hmmm... not all systems have a fmt program; should we
    # just go ahead and use ``nroff -Tcrt | cat -s'' here?
    fmt='nroff -Tcrt | cat -s'
    #fmt=fmt ;

    case "$option" in
    -catalog*)
        #
        # -catalog [proto ...]
        #
        echo "\
Catalog of ${name} class templates
directories searched:
    $PROTODIR
    $pwd
selecting: $select
classes available:" ;
        ;;
    -list*)
        #
        # -list [proto ...]
        #
        # no need to do anything (the list is coming out next)
        ;;
    esac ;

# The sed script does the following:
# - If it does not end in a .ccP or .hP then
#   it's not a template and we are not intereseted.
# - Get rid of pathname components [s;.*/;;]
# - Just take the template names
# - change quoting conventions and select off what we want to see
# -if it did not pass the patterns, kill it

    ls $pwd $PROTODIR | sed -e '
/\.ccP$/ !{
   /\.hP$/ !{
     d
   }
}
s;.*/;;
s/\.ccP$//
s/\.hP$//
' -e "$select_pattern
d
" | sort -u | case "$option" in
    -catalog*)
        # The library catalog information preceded the list
        # format the list, and tab in in a bit to make it readable.
        # Re-evaluate $fmt because it might contain a shell command
        eval $fmt | sed -e 's/.*/    &/' ;
        ;;
    -list*)
        # nothing special, just let the sorted list dribble out
        # we must use cat to receive (and reproduce) the incoming list
        cat ;
        ;;
    esac ;
    exit 0;
    ;;
-2)
    #
    # genclass -2 type1 {ref|val} type2 {ref|val} proto [out_prefix]
    #
    N=2 ;

    case $# in
    6) # genclass -2 type1 {ref|val} type2 {ref|val} proto
       ;;
    7) # genclass -2 type1 {ref|val} type2 {ref|val} proto out_prefix
       ;;
    *)
	echo "usage: $usage" 1>&2 ;
	exit 1;
    esac ;
    shift ;
    ;;
*)
    #
    # genclass type1 {ref|val} proto [out_prefix]
    #
    N=1 ;

    case $# in
    3) # genclass type1 {ref|val} proto
       ;;
    4) # genclass type1 {ref|val} proto out_prefix
       ;;
    *)
	echo "usage: $usage" 1>&2 ;
	exit 1;
    esac ;
    ;;
esac

#
# Args are now (the point being the leading ``-2'' is gone)
#
#     type1 {ref|val} proto [out_prefix]
#     type1 {ref|val} type2 {ref|val} proto [out_prefix]
#

#
# Quote all of the $1 $2 etc references to guard against
# dynamic syntax errors due to vacuous arguments (i.e. '')
# as sometimes occurs when genclass is used from a Makefile
#

T1="$1";
T1NAME="$T1." ;
T1SEDNAME="$T1" ;

case "$2" in
ref)
     T1ACC="\&" ;
     ;;
val)
     T1ACC=" " ;
     ;;
*)
    echo "${name}: Must specify type1 access as ref or val" 1>&2 ;
    echo "usage: $usage" 1>&2 ;
    exit 1;
    ;;
esac

# N is either 1 or 2

case $N in
1)
    #
    # type1 {ref|val} proto [out_prefix]
    #
    class="$3" ;

    T2="" ;
    T2ACC="" ;
    ;;
2)
    #
    # type1 {ref|val} type2 {ref|val} proto [out_prefix]
    #
    class="$5" ;

    T2="$3";
    T2NAME="$T2." ;
    T2SEDNAME="$T2" ;

    case "$4" in
    ref)
        T2ACC="\&" ;
        ;;
    val)
        T2ACC=" " ;
        ;;
     *)
        echo "${name}: Must specify type2 access: ref or val" 1>&2 ;
	echo "usage: $usage" 1>&2 ;
	exit 1;;
    esac;
    ;;
esac

defaultprefix="$T1NAME$T2NAME" ;

case $# in
3)  # type1 {ref|val} proto
    replaceprefix="N" ;
    prefix="$defaultprefix" ;
    ;;
5)  # type1 {ref|val} type2 {ref|val} proto
    replaceprefix="N" ;
    prefix="$defaultprefix" ;
    ;;
4)  # type1 {ref|val} proto out_prefix
    prefix="$4" ;
    replaceprefix="Y" ;
    ;;
6)  # type1 {ref|val} type2 {ref|val} proto out_prefix
    prefix="$6" ;
    replaceprefix="Y" ;
    ;;
*)
    echo "${name}: too many arguments" 1>&2 ;
    echo "usage: $usage" 1>&2 ;  
    exit 1;
    ;;
esac ;

src_h=$class.hP
src_cc=$class.ccP
out_h=$prefix$class.h;
out_cc=$prefix$class.cc ;

#
# Note #1: The .h and .cc parts are done separately
#     in case only a .h exists for the prototype
#
# Note #2: Bind the .h and .cc parts to the fullpath
#     directories at the same time to ensure consistency.
#

if [ -f $pwd/$src_h ] ; then
    fullsrc_h=$pwd/$src_h ;
    fullsrc_cc=$pwd/$src_cc ;
elif [ -f $PROTODIR/$src_h ] ; then
    fullsrc_h=$PROTODIR/$src_h ;
    fullsrc_cc=$PROTODIR/$src_cc ;
else
    echo "${name}: there is no prototype for class $class - file $src_h" 1>&2 ;
    $0 -list ;
    exit 1;
fi

CASES="$N$replaceprefix" ;
# CASES is one of { 2Y 2N 1Y 1N }

#
# WATCHOUT - we have no way of checking whether or not
# the proper case type is being used with the prototype.
#
# For example, we have no way of ensuring that any of
# Map variants are specified with the -2 argument set
# Further, we have no way of ensuring that -2 is not
# used with the prototypes which require only one.
#
# The second problem is not serious because it still
# results in correctly-generated C++ code; the first
# problem is serious because it results in C++ code that
# still has ``<C>'' and ``<C&>'' syntax inside it.  Such
# code of course will not compile.
#
# SO THE BEST WE CAN DO - is check for the presence of
# <C> and <C&> AFTER the thing has been generated.
#

case $CASES in
2Y) # Two output substitutions, change the prefix
    sed < $fullsrc_h > $out_h -e "
s/<T>/$T1/g
s/<T&>/$T1$T1ACC/g
s/<C>/$T2/g
s/<C&>/$T2$T2ACC/g
s/$T1SEDNAME\.$T2SEDNAME\./$prefix/g
s/$T1SEDNAME\./$prefix/g
s/$T2SEDNAME\./$prefix/g
" ;
    ;;
2N) # Two output substitutions, use the default prefix
    sed < $fullsrc_h > $out_h -e "
s/<T>/$T1/g
s/<T&>/$T1$T1ACC/g
s/<C>/$T2/g
s/<C&>/$T2$T2ACC/g
" ;
    ;;
1Y) # One output substitution, change the prefix
    sed < $fullsrc_h > $out_h -e "
s/<T>/$T1/g
s/<T&>/$T1$T1ACC/g
s/$T1SEDNAME\./$prefix/g
" ;
    ;;
1N) # One output substitution, use the default prefix
    sed < $fullsrc_h > $out_h -e "
s/<T>/$T1/g
s/<T&>/$T1$T1ACC/g
" ;
    ;;
esac

if egrep '<C&?>' $out_h > /dev/null ; then
    echo "${name}: the $class class requires the -2 syntax for the 2nd type" 1>&2 ;
    echo "usage: $usage" 1>&2 ;
    # the user does not get to see the mistakes (he might try to compile it)
    rm $out_h ;
    exit 1;
fi ;

if [ ! -f $fullsrc_cc ] ; then
    echo "${name}: warning, class has a .h but no .cc file" 1>&2 ;
    exit 0;
fi

case $CASES in
2Y) # Two output substitutions, change the prefix
    sed < $fullsrc_cc > $out_cc -e "
s/<T>/$T1/g
s/<T&>/$T1$T1ACC/g
s/<C>/$T2/g
s/<C&>/$T2$T2ACC/g
s/$T1SEDNAME\.$T2SEDNAME\./$prefix/g
s/$T1SEDNAME\./$prefix/g
s/$T2SEDNAME\./$prefix/g
"
    ;;
2N) # Two output substitutions, use the default prefix
    sed < $fullsrc_cc > $out_cc -e "
s/<T>/$T1/g
s/<T&>/$T1$T1ACC/g
s/<C>/$T2/g
s/<C&>/$T2$T2ACC/g
"
    ;;
1Y) # One output substitution, change the prefix
    sed < $fullsrc_cc > $out_cc -e "
s/<T>/$T1/g
s/<T&>/$T1$T1ACC/g
s/$T1SEDNAME\./$prefix/g
"
    ;;
1N) # One output substitution, use the default prefix
    sed < $fullsrc_cc > $out_cc -e "
s/<T>/$T1/g
s/<T&>/$T1$T1ACC/g
"
    ;;
esac

if egrep '<C&?>' $out_h $out_cc > /dev/null ; then
    echo "${name}: the $class class requires the -2 syntax for the 2nd type" 1>&2 ;
    echo "usage: $usage" 1>&2 ;
    # the user does not get to see the mistakes (he might try to compile it)
    rm $out_h $out_cc ;
    exit 1;
fi ;

exit 0;
