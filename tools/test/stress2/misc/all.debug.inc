#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2020 Peter Holm <pho@FreeBSD.org>
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
#

# Example debug include file for misc/all.sh

pre_debug() {
	debug_statfs=`vmstat -m | grep -w statfs | awk '{print $2}'`
	debug_freework=`vmstat -m | grep -w freework | awk '{print $2}'`
	debug_newblk=`vmstat -m | grep -w newblk | awk '{print $2}'`
	debug_pts=`vmstat -m | grep -w pts | awk '{print $2}'`
	debug_routetbl=`vmstat -m | grep -w routetbl | awk '{print $2}'`
	debug_mount=`vmstat -m | awk '/  mount/{print $2}'`
	debug_geom=`vmstat -m | grep -w GEOM | awk '{print $2}'`
#	debug_indirdep=`vmstat -m | grep -w indirdep | awk '{print $2}'`
}

post_debug() {
	local debug_new

	debug_new=`vmstat -m | grep -w statfs | awk '{print $2}'`
	[ $((debug_new - debug_statfs)) -gt 0 ] &&
	    { printf "stress2: statfs leak: %d/%d.\r\n" \
	        $((debug_new - debug_statfs)) $debug_new > $console
		debug_statfs=$debug_new; }

	debug_new=`vmstat -m | grep -w freework | awk '{print $2}'`
	[ $((debug_new - debug_freework)) -gt 0 -a $debug_new -gt 100 ] &&
	    { printf "stress2: freework leak: %d/%d.\r\n" \
	        $((debug_new - debug_freework)) $debug_new > $console
		debug_freework=$debug_new; }

	debug_new=`vmstat -m | grep -w newblk | awk '{print $2}'`
	[ $((debug_new - debug_newblk)) -gt 0 -a $debug_new -gt 100 ] &&
	    { printf "stress2: newblk leak: %d/%d.\r\n" \
	        $((debug_new - debug_newblk)) $debug_new > $console
		debug_newblk=$debug_new; }

	debug_new=`vmstat -m | grep -w pts | awk '{print $2}'`
	[ $((debug_new - debug_pts)) -gt 0 ] &&
	    { printf "stress2: pts leak: %d/%d\r\n" \
	        $((debug_new - debug_pts)) $debug_new > $console
		debug_pts=$debug_new; }

	debug_new=`vmstat -m | grep -w routetbl | awk '{print $2}'`
	[ $((debug_new - debug_routetbl)) -gt 0 ] &&
	    { printf "stress2: routetbl leak: %d/%d\r\n" \
	        $((debug_new - debug_routetbl)) $debug_new > $console
		debug_routetbl=$debug_new; }

	debug_new=`vmstat -m | awk '/  mount/{print $2}'`
	[ $((debug_new - debug_mount)) -gt 0 ] &&
	    { printf "stress2: mount leak: %d/%d\r\n" \
	        $((debug_new - debug_mount)) $debug_new > $console
		debug_mount=$debug_new; }

	debug_new=`vmstat -m | awk '/GEOM/{print $2}'`
	[ $((debug_new - debug_geom)) -gt 0 ] &&
	    { printf "stress2: GEOM leak: %d/%d\r\n" \
	        $((debug_new - debug_geom)) $debug_new > $console
		debug_geom=$debug_new; }

#	debug_new=`vmstat -m | awk '/indirdep/{print $2}'`
#	[ $((debug_new - debug_indirdep)) -gt 100 -a $debug_new -gt 100 ] &&
#	    { printf "stress2: indirdep leak: %d/%d\r\n" \
#	        $((debug_new - debug_indirdep)) $debug_new > $console
#		debug_indirdep=$debug_new; }
}

all_debug=1
