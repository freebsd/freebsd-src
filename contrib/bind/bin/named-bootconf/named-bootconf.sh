#!/bin/sh
#
# $NetBSD: named-bootconf.sh,v 1.5 1998/12/15 01:00:53 tron Exp $
#
# Copyright (c) 1995, 1998 The NetBSD Foundation, Inc.
# All rights reserved.
#
# This code is derived from software contributed to The NetBSD Foundation
# by Matthias Scheler.
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
#	This product includes software developed by the NetBSD
#	Foundation, Inc. and its contributors.
# 4. Neither the name of The NetBSD Foundation nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

## Copyright (c) 1999 by Internet Software Consortium
##
## Permission to use, copy, modify, and distribute this software for any
## purpose with or without fee is hereby granted, provided that the above
## copyright notice and this permission notice appear in all copies.
##
## THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
## ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
## OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
## CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
## DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
## PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
## ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
## SOFTWARE.

if [ ${OPTIONFILE-X} = X ]; then
	OPTIONFILE=/tmp/.options.`date +%s`.$$
	ZONEFILE=/tmp/.zones.`date +%s`.$$
	COMMENTFILE=/tmp/.comments.`date +%s`.$$
	export OPTIONFILE ZONEFILE COMMENTFILE
	touch $OPTIONFILE $ZONEFILE $COMMENTFILE
	DUMP=1
else
	DUMP=0
fi

while read CMD ARGS; do
	class=
	CMD=`echo "${CMD}" | tr '[A-Z]' '[a-z]'`
	case $CMD in
	\; )
		echo \# $ARGS >>$COMMENTFILE
		;;
	cache )
		set - X $ARGS
		shift
		if [ $# -eq 2 ]; then
			(echo ""
			cat $COMMENTFILE
			echo "zone \"$1\" {"
			echo "	type hint;"
			echo "	file \"$2\";"
			echo "};") >>$ZONEFILE
			rm -f $COMMENTFILE
			touch $COMMENTFILE
		fi
		;;
	directory )
		set - X $ARGS
		shift
		if [ $# -eq 1 ]; then
			(cat $COMMENTFILE
			echo "	directory \"$1\";") >>$OPTIONFILE
			rm -f $COMMENTFILE
			touch $COMMENTFILE

			DIRECTORY=$1
			export DIRECTORY
		fi
		;; 
	forwarders )
		(cat $COMMENTFILE
		echo "	forwarders {"
		for ARG in $ARGS; do
			echo "		$ARG;"
		done
		echo "	};") >>$OPTIONFILE
		rm -f $COMMENTFILE
		touch $COMMENTFILE
		;;
	include )
		if [ "$ARGS" != "" ]; then
			(cd ${DIRECTORY-.}; cat $ARGS) | $0
		fi
		;;
	limit )
		ARGS=`echo "${ARGS}" | tr '[A-Z]' '[a-z]'`
		set - X $ARGS
		shift
		if [ $# -eq 2 ]; then
			cat $COMMENTFILE >>$OPTIONFILE
			case $1 in
			datasize | files | transfers-in | transfers-per-ns )
				echo "	$1 $2;" >>$OPTIONFILE
				;;
			esac
			rm -f $COMMENTFILE
			touch $COMMENTFILE
		fi
		;;
	options )
		ARGS=`echo "${ARGS}" | tr '[A-Z]' '[a-z]'`
		cat $COMMENTFILE >>$OPTIONFILE
		for ARG in $ARGS; do
			case $ARG in
			fake-iquery )
				echo "	fake-iquery yes;" >>$OPTIONFILE
				;;
			forward-only )
				echo "	forward only;" >>$OPTIONFILE
				;;
			no-fetch-glue )
				echo "	fetch-glue no;" >>$OPTIONFILE
				;;
			no-recursion )
				echo "	recursion no;" >>$OPTIONFILE
				;;
			no-round-robin ) # HP extention
				echo "	rrset-order {" >>$OPTIONFILE
				echo "		class ANY type ANY name \"*\" order fixed;" >>$OPTIONFILE
				echo "	};" >>$OPTIONFILE
				;;
			esac
		done
		rm -f $COMMENTFILE
		touch $COMMENTFILE
		;;
	primary|primary/* )
		case $CMD in
		primary/chaos )
			class="chaos "
			;;
		primary/hs )
			class="hesiod "
			;;
		esac
		set - X $ARGS
		shift
		if [ $# -eq 2 ]; then
			(echo ""
			cat $COMMENTFILE
			echo "zone \"$1\" ${class}{"
			echo "	type master;"
			echo "	file \"$2\";"
			echo "};") >>$ZONEFILE
			rm -f $COMMENTFILE
			touch $COMMENTFILE
		fi
		;;
	secondary|secondary/* )
		case $CMD in
		secondary/chaos )
			class="chaos "
			;;
		secondary/hs )
			class="hesiod "
			;;
		esac
		set - X $ARGS
		shift
		if [ $# -gt 1 ]; then
			ZONE=$1
			shift
			PRIMARIES=""
			while [ $# -gt 1 ]; do
				PRIMARIES="$PRIMARIES $1"
				shift
			done
			(echo ""
			cat $COMMENTFILE
			echo "zone \"$ZONE\" ${class}{"
			echo "	type slave;"
			if expr x"$1" : '^x[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*$' > /dev/null
			then
				PRIMARIES="$PRIMARIES $1"
			else
				echo "	file \"$1\";"
			fi
			echo "	masters {"
			for PRIMARY in $PRIMARIES; do
				echo "		$PRIMARY;"
			done
			echo "	};"
			echo "};") >>$ZONEFILE
			rm -f $COMMENTFILE
			touch $COMMENTFILE
		fi
		;;
	stub|stub/* )
		case $CMD in
		stub/chaos )
			class="chaos "
			;;
		stub/hs )
			class="hesiod "
			;;
		esac
		set - X $ARGS
		shift
		if [ $# -gt 1 ]; then
			ZONE=$1
			shift
			PRIMARIES=""
			while [ $# -gt 1 ]; do
				PRIMARIES="$PRIMARIES $1"
				shift
			done
			(echo ""
			cat $COMMENTFILE
			echo "zone \"$ZONE\" ${class}{"
			echo "	type stub;"
			if expr x"$1" : '^x[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*$' > /dev/null
			then
				PRIMARIES="$PRIMARIES $1"
			else
				echo "	file \"$1\";"
			fi
			echo "	masters {"
			for PRIMARY in $PRIMARIES; do
				echo "		$PRIMARY;"
			done
			echo "	};"
			echo "};") >>$ZONEFILE
			rm -f $COMMENTFILE
			touch $COMMENTFILE
		fi
		;;
	slave )
		cat $COMMENTFILE >>$OPTIONFILE
		echo "	forward only;" >>$OPTIONFILE
		rm -f $COMMENTFILE
		touch $COMMENTFILE
		;;
	sortlist )
		(cat $COMMENTFILE
		echo "	topology {"
		for ARG in $ARGS; do
			case $ARG in
			*.0.0.0 )
				echo "		$ARG/8;"
				;;
			*.0.0 )
				echo "		$ARG/16;"
				;;
			*.0 )
				echo "		$ARG/24;"
				;;
			* )
				echo "		$ARG;"
				;;
			esac
		done
		echo "	};") >>$OPTIONFILE
		rm -f $COMMENTFILE
		touch $COMMENTFILE
		;;
	tcplist | xfrnets )
		(cat $COMMENTFILE
		echo "	allow-transfer {"
		for ARG in $ARGS; do
			case $ARG in
			*.0.0.0 )
				echo "		$ARG/8;"
				;;
			*.0.0 )
				echo "		$ARG/16;"
				;;
			*.0 )
				echo "		$ARG/24;"
				;;
			* )
				echo "		$ARG;"
				;;
			esac
		done
		echo "	};") >>$OPTIONFILE
		rm -f $COMMENTFILE
		touch $COMMENTFILE
		;;
	esac
done

if [ $DUMP -eq 1 ]; then
	echo ""
	echo "options {"
	cat $OPTIONFILE
	echo "};"
	cat $ZONEFILE $COMMENTFILE

	rm -f $OPTIONFILE $ZONEFILE $COMMENTFILE
fi

exit 0
