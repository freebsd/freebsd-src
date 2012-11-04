#!/bin/sh
#-
# Copyright (c) 2012 Eitan Adler
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer
#    in this position and unchanged.
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

usage() {
	echo "usage: ssh-copy-id [-l] [-i keyfile] [-o option] [-p port] [user@]hostname" >&2
	exit 1
}

sendkey() {
	local h="$1"
	shift 1
	local k="$@"
	echo "$k" | ssh $port -S none $options "$user$h" /bin/sh -c \''
		set -e;
		umask 077;
		keyfile=$HOME/.ssh/authorized_keys ;
		mkdir -p $HOME/.ssh/ ;
		while read alg key comment ; do
			if ! grep -sqwF "$key" "$keyfile"; then
				echo "$alg $key $comment" |
				    tee -a "$keyfile" >/dev/null ;
			fi ;
		done
	'\' 
}

agentKeys() {
	keys="$(ssh-add -L | grep -v 'The agent has no identities.')$nl$keys"
}

keys=""
host=""
hasarg=""
user=""
port=""
nl="
"
options=""

while getopts 'i:lo:p:' arg; do
	case $arg in
	i)	
		hasarg="x"
		if [ -f "$OPTARG" ]; then
			keys="$(cat $OPTARG)$nl$keys"
		fi
		;;
	l)	
		hasarg="x"
		agentKeys
		;;
	p)	
		port="-p $OPTARG"
		;;
	o)	
		options="$options -o '$OPTARG'"
		;;
	*)	
		usage
		;;
	esac
done >&2

shift $((OPTIND-1))

if [ -z "$hasarg" ]; then
	agentKeys
fi
if [ -z "$keys" -o "$keys" = "$nl" ]; then
	echo "no keys found" >&2
	exit 1
fi
if [ -z "$@" ]; then
	usage
fi

for host in "$@"; do
	sendkey "$host" "$keys"
done
