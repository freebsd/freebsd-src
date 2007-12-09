#! /bin/sh
#-
# Copyright (c) 2007 David O'Brien <obrien@FreeBSD.org>
# Copyright (c) 2006 Matthias Schmidt <schmidtm @ mathematik . uni-marburg.de>
#
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# - Redistributions of source code must retain the above copyright notice,
#   this list of conditions and the following disclaimer.
# - Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# $FreeBSD$
#

PORTSDIR=${PORTSDIR:-/usr/ports}
id=`uname -r | cut -d '.' -f 1`
INDEXFILE=${INDEXFILE:-INDEX-$id}

if [ -z $1 ]; then
	echo "Usage: $0 [ -i | -k | -r] <name>"
	exit 1
fi

if [ ! -d $PORTSDIR ]; then
	echo "No Ports Tree Found!  Please install."
	exit 1
fi

case "$1" in
-i)
	awk -F\| -v name="$2" \
	    '{\
		if ($1 ~ name) { \
			split($2, a, "/"); \
			printf("Name\t: %s-50\nDir\t: %-50s\nDesc\t: %-50s\nURL\t: %-50s\nDeps\t: %s\n\n", $1, $2, $4, $10, $9); \
		}
	    }' ${PORTSDIR}/${INDEXFILE}
	;;
-k)
	awk -F\| -v name="$2" \
	    '{\
		if ($1 ~ name || $4 ~ name || $10 ~ name) { \
			split($2, a, "/"); \
			printf("%-20s\t%-25s\n", $1, $4); \
		}
	    }' ${PORTSDIR}/${INDEXFILE}

	;;
-r)
	awk -F\| -v name="$2" \
	    '{\
		if ($1 ~ name) { \
			split($2, a, "/"); \
			printf("%-20s\t%s\n", $1, $2); \
		}
	    }' ${PORTSDIR}/${INDEXFILE}
	;;
*)
	awk -F\| -v name="$1" \
	    '{\
		if ($1 ~ name) { \
			split($2, a, "/"); \
			printf("%-20s\t%-25s\n", $1, $4); \
		}
	    }' ${PORTSDIR}/${INDEXFILE}
	;;
esac

exit $?
