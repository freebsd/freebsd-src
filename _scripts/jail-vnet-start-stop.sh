#!/bin/sh
#-
# Copyright (c) 2016 The FreeBSD Foundation
# All rights reserved.
#
# This software was developed by Björn Zeeb under
# the sponsorship from the FreeBSD Foundation.
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
# $FreeBSD$
#

max=$1
case "${max}" in
"")	max=9999999
	;;
esac

prefix=/tmp/`date +%s`

i=0

ending()
{
#	vmstat -m > ${prefix}-vmstat-m-2
#	echo "diff -up ${prefix}-vmstat-m-?"
#	vmstat -z > ${prefix}-vmstat-z-2
#	echo "diff -up ${prefix}-vmstat-z-?"

	exit 0
}

#sysctl security.jail.vimage_debug_memory=0
#sysctl security.jail.vimage_debug_memory=2

#vmstat -z > ${prefix}-vmstat-z-1
#vmstat -m > ${prefix}-vmstat-m-1

while : ; do

	i=$((i+1))
	x=`expr ${i} % 100`
	case ${x} in
	0)	echo "`date` try ${i}" ;;
	esac
	id=`jail -i -c vnet persist`
	#echo "ID=${id}"
	jail -r ${id}
	
	if test ${id} -ge ${max} ; then
		echo "Reached max; ending.."
		ending
	fi
done

ending

# end
