#!/bin/sh
#
# Copyright (c) 2001  The FreeBSD Project
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

# This script is called by cron to store bits of randomness which are
# then used to seed /dev/random on boot.

PATH=/bin:/usr/bin

# If there is a global system configuration file, suck it in.
#
if [ -r /etc/defaults/rc.conf ]; then
	. /etc/defaults/rc.conf
	source_rc_confs
elif [ -r /etc/rc.conf ]; then
	. /etc/rc.conf
fi

case ${entropy_dir} in
[Nn][Oo])
	exit 0
	;;
*)
	entropy_dir=${entropy_dir:-/.entropy}
	;;
esac

entropy_save_sz=${entropy_save_sz:-2048}
entropy_save_num=${entropy_save_num:-8}
entropy_save_jot=$(($entropy_save_num - 1))

if [ ! -d "${entropy_dir}" ]; then
	umask 077
	mkdir "${entropy_dir}" || {
	    logger -is The entropy directory "${entropy_dir}" does not \
exist, and cannot be created.  Therefore no entropy can be saved. ;
	    exit 1;}
	/usr/sbin/chown operator:operator "${entropy_dir}"
	chmod 0700 "${entropy_dir}"
fi

rm -f "${entropy_dir}/saved-entropy.${entropy_save_num}"

umask 377

for file_num in `jot ${entropy_save_jot} ${entropy_save_jot} 1`; do
	if [ -f "${entropy_dir}/saved-entropy.${file_num}" ]; then
		new_num=$(($file_num + 1))
		mv "${entropy_dir}/saved-entropy.${file_num}" \
		    "${entropy_dir}/saved-entropy.${new_num}"
	fi
done

dd if=/dev/random of="${entropy_dir}/saved-entropy.1" \
    bs=2048 count=1 2> /dev/null

exit 0

