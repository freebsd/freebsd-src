#!/bin/sh
#
# Copyright (c) 1990, 1993
#	The Regents of the University of California.  All rights reserved.
#
# This code is derived from software contributed to Berkeley by
# the Systems Programming Group of the University of Utah Computer
# Science Department.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#	This product includes software developed by the University of
#	California, Berkeley and its contributors.
# 4. Neither the name of the University nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
#	@(#)cpp.sh	8.1 (Berkeley) 6/6/93
#
# Transitional front end to CCCP to make it behave like (Reiser) CCP:
#	specifies -traditional
#	doesn't search gcc-include
#
PATH=/usr/bin:/bin
CPP=/usr/libexec/gcc2/cpp
ALST="-D__GNUC__ -$ "
NSI=no
OPTS=""
INCS="-nostdinc"
FOUNDFILES=no

for A
do
	case $A in
	-nostdinc)
		NSI=yes
		;;
	-traditional)
		;;
	-I*)
		INCS="$INCS $A"
		;;
	-U__GNUC__)
		ALST=`echo $ALST | sed -e 's/-D__GNUC__//'`
		;;
	-*)
		OPTS="$OPTS '$A'"
		;;
	*)
		FOUNDFILES=yes
		if [ $NSI = "no" ]
		then
			INCS="$INCS -I/usr/include"
			NSI=skip
		fi
		eval $CPP $ALST $INCS $LIBS $CSU $OPTS $A || exit $?
		;;
	esac
done

if [ $FOUNDFILES = "no" ]
then
	# read standard input
	if [ $NSI = "no" ]
	then
		INCS="$INCS -I/usr/include"
	fi
	eval exec $CPP $ALST $INCS $LIBS $CSU $OPTS
fi

exit 0
