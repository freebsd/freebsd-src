...using script...
CVS: up -A -p c89.sh
#!/bin/sh
#
# Copyright (c) 1997 Joerg Wunsch
#
# All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY THE DEVELOPERS ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE DEVELOPERS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#	$Id: c89.sh,v 1.3 1997/10/05 18:44:37 helbig Exp $
#
# This is the Posix.2 mandated C compiler.  Basically, a hook to the
# cc(1) command.

usage()
{
	echo "usage: c89 [-c] [-D name[=value]] [...] [-E] [-g] [-I directory ...]
       [-L directory ...] [-o outfile] [-O] [-s] [-U name ...] operand ..." 1>&2
	exit 64
}

_PARAMS="$@"

while getopts "cD:EgI:L:o:OsU:" opt
do
	case $opt in
	[cDEgILoOsU])
		;;
	*)
		usage
		;;
	esac
done

shift $(($OPTIND - 1))

if [ $# = "0" ]
then
	echo "Missing operand" 1>&2
	usage
fi

while [ $# != "0" ]
do
	case $1 in
	-l* | *.a | *.c | *.o)
		shift
		;;
	*)
		echo "Invalid operand" 1>&2
		usage
		;;
	esac
done

exec cc -ansi -pedantic -D_ANSI_SOURCE $_PARAMS
