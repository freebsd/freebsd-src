#!/bin/sh
# ----------------------------------------------------------------------------
# "THE BEER-WARE LICENSE" (Revision 42):
# <phk@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
# can do whatever you want with this stuff. If we meet some day, and you think
# this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
# ----------------------------------------------------------------------------
#
# $FreeBSD: src/tools/tools/kerninclude/kerninclude.sh,v 1.1 1999/10/11 19:43:44 phk Exp $
#
# This script tries to find #include statements which are not needed in
# the FreeBSD kernel tree.
#
# For each include file on the tasklist (set in $includes right below)
#    For each object file (see around line 170 for how these are selected)
#	For each kernel (set in $kernels right below) and all modules
#	   if the object exists
#	      figure out the sourcefile
#	      if the sourcefile doesn't contain "#include $include"
#	          continue
#	      if object can be compile without $include existing
#	          continue /* probably protected by #ifdef something */
#	      if object can't be compile with empty file for $include 
#	          continue /* needed something/
#	      if the compiler warnings/errors were different than normal
#	          continue /* needed something/
#	      if the resulting object were different than normal
#	          continue /* needed something */
#             /* didn't need this include */
#	      remove $include from source file
#
# Takes about 12h to run on a PII/400
#
# NOTE: /usr/include is mucked about with!!
#

set -e

# Base of the kernel sources you want to work on
cd /some/sourcetree/sys

# Set to true to start from scratch, false to resume
init=true

# Which kernels you want to check
kernels="LINT GENERIC GENERIC98"

# Which includes you want to check
includes="*/*.h i386/*/*.h dev/*/*.h cam/scsi/*.h ufs/*/*.h pc98/*/*.h netatm/*/*.h i4b/*/*.h"

check_it ()
{
	if [ -f ::$2 ] ; then
		if grep "#[ 	]*include[ 	].$1." ::$2 > /dev/null; then
			src=`ls -l ::$2 | awk '{print $11}'`
		else
			echo " -"
			exit 0
		fi
	else
		rm -f $2
		make -n $2 > _0 2>&1 || true
		src=`awk '$1 == "cc" {print $NF}' _0`
		if expr "x$src" : 'x.*\.c$' > /dev/null ; then
			ln -s $src ::$2
		else
			echo " not C source"
			# don't create $2, we don't care about it.
			exit 0
		fi
		if grep "#[ 	]*include[ 	].$1." $src > /dev/null; then
			true
		else
			echo " -"
			touch $2
			exit 0
		fi
	fi
	rm ../../$1
	rm -f $2
	if [ -f /usr/include/$1 ] ; then
		mv /usr/include/$1 /usr/include/${1}_
		make $2 > _0 2>&1 || true
		mv /usr/include/${1}_ /usr/include/$1
	else
		make $2 > _0 2>&1 || true
	fi
	echo > ../../$1
	if [ -f $2 ] ; then
		echo " no read"
		cp ../../${1}_ ../../$1
		exit 0
	fi

	make $2 > _1 2>&1 || true

	cp ../../${1}_ ../../$1
	
	if [ ! -f $2 ] ; then
		echo " compile error"
		touch $2
		exit 0
	fi
	m2=`md5 < $2`

	rm $2
	make $2 > _0 2>&1 || true
	if [ ! -f $2 ] ; then
		echo "$2 reference compile failed"
		touch $2
		cat _0
		exit 0
	fi
	m1=`md5 < $2`

	if cmp _0 _1 > /dev/null 2>&1 ; then
		true
	else
		echo " warnings changed"
		exit 0
	fi

	if [ $m1 != $m2 ] ; then
		echo " MD5 changed"
		exit 0
	fi
}


if $init ; then
	(
	cd modules
	make clean > /dev/null 2>&1
	make cleandir > /dev/null 2>&1
	make cleandir > /dev/null 2>&1
	make clean > /dev/null 2>&1
	make clean > /dev/null 2>&1
	)

	(
	cd compile
	ls | grep -v CVS | xargs rm -rf
	)

	( 
	cd i386/conf
	for i in $kernels
	do
		if [ -f $i ] ; then
			config -r $i
		fi
	done
	cd ../../pc98/conf
	for i in $kernels
	do
		if [ -f $i ] ; then
			config -r $i
		fi
	done
	)

	for i in $kernels
	do
		(
		echo "Compiling $i"
		cd compile/$i
		rm -f ::*
		make -k > x.0 2>&1
		tail -4 x.0
		)
	done

	(
	echo "Compiling modules"
	cd modules
	rm -f */::*
	make -k > x.0 2>&1 || true
	)
fi

# Generate the list of object files we want to check
# you can put a convenient grep right before the sort
# if you want just some specific subset of files checked
(
cd modules
for i in *
do
	if [ -d $i -a $i != CVS ] ; then
		( cd $i ; ls *.o 2>/dev/null || true)
	fi
done
cd ../compile
for i in $kernels
do
	( cd $i ; ls *.o 2>/dev/null )
done
) | sed '
/aicasm/d	
/genassym/d
/vers.o/d
/setdef0.o/d
/setdef1.o/d
/::/d
' | sort -u > _

objlist=`cat _`

find . -name '*.h_' -print | xargs rm -f

for incl in $includes
do
	if [ ! -f ${incl} ] ; then
		continue
	fi
	if [ ! -f ${incl}_ ] ; then
		cp $incl ${incl}_
	fi
	for obj in $objlist
	do
		(
		echo -n "$incl $obj:"
		src=""

		cd compile 
		for i in $kernels
		do
			cd $i

			if [ ! -f $obj ] ; then
				cd ..
				continue
			fi
			echo -n " [$i]"
			check_it $incl $obj
			cd ..
		done
		cd ..
		cd modules
		for d in */$obj
		do
			if [ ! -f $d ] ; then
				continue
			fi
			b=`dirname $d`
			echo -n " [$b]"
			cd $b
			check_it $incl $obj 
			cd ..
		done
		cd ..
		if [ "x$src" = "x" ] ; then
			echo " -"
			exit 0
		fi
		echo -n " ($src)"
		echo " BINGO!"
		echo "$incl $src" >> _incl
		(
		cd compile/LINT
		grep -v "#[ 	]*include[ 	]*.$incl." $src > ${src}_
		mv ${src}_ $src
		)
		)
	done
done

