#!/bin/sh
#
#
# Copyright (c) 1995 Joerg Wunsch
#
# All rights reserved.
#
# This program is free software.
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
#	This product includes software developed by Joerg Wunsch
# 4. The name of the developer may not be used to endorse or promote
#    products derived from this software without specific prior written
#    permission.
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
#
# Posix 1003.2 compliant print spooler interface.
#
# $Id: lp.sh,v 1.2 1995/08/06 19:39:38 joerg Exp $
#

ncopies=""
symlink="-s"

# Posix says LPDEST gets precedence over PRINTER
dest=${LPDEST:-${PRINTER:-lp}}

#
# XXX We include the -o flag as a dummy.  Posix 1003.2 does not require
# it, but the rationale mentions it as a possible future extension.
#
while getopts "cd:n:o:" option
do
	case $option in

	c)			# copy files before printing
		symlink="";;
	d)			# destination
		dest="${OPTARG}";;
	n)			# number of copies
		ncopies="-#${OPTARG}";;
	o)			# (printer option)
		: ;;
	*)			# (error msg printed by getopts)
		exit 2;;
	esac
done

shift $(($OPTIND - 1))

exec /usr/bin/lpr "-P${dest}" ${symlink} ${ncopies} "$@"
