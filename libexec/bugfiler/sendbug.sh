#!/bin/sh -
#
# Copyright (c) 1983, 1993
#	The Regents of the University of California.  All rights reserved.
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
#	@(#)sendbug.sh	8.1 (Berkeley) 6/4/93
#

# create a bug report and mail it to '4bsd-bugs'.

PATH=/bin:/sbin:/usr/sbin:/usr/bin
export PATH

TEMP=/tmp/bug$$
FORMAT=/usr/share/misc/bugformat

# uucp sites should use ": ${BUGADDR=ucbvax!4bsd-bugs}" with a suitable path.
: ${BUGADDR=4bsd-bugs@CS.Berkeley.EDU}
: ${EDITOR=vi}

trap 'rm -f $TEMP ; exit 1' 1 2 3 13 15

cp $FORMAT $TEMP
chmod u+w $TEMP
if $EDITOR $TEMP
then
	if cmp -s $FORMAT $TEMP
	then
		echo "File not changed, no bug report submitted."
		exit
	fi
	case "$#" in
	0) sendmail -t -oi $BUGADDR  < $TEMP ;;
	*) sendmail -t -oi "$@" < $TEMP ;;
	esac
fi

rm -f $TEMP
