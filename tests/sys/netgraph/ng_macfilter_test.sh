#!/bin/sh
#
# Copyright (c) 2018-2020 Retina b.v.
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
# 3. Neither the name of the University nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
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

progname="$(basename $0 .sh)"
entries_lst="/tmp/$progname.entries.lst"
entries2_lst="/tmp/$progname.entries2.lst"

HOOKS=3
HOOKSADD=42
ITERATIONS=7
SUBITERATIONS=71

find_iface () {
	# Figure out the first ethernet interface
	ifconfig -u -l ether | awk '{print $1}'
}

loaded_modules=''
load_modules () {
	for kmod in $*; do
		if ! kldstat -q -m $kmod; then
			test_comment "Loading $kmod..."
			kldload $kmod
			loaded_modules="$loaded_modules $kmod"
		fi
	done
}
unload_modules () {
	for kmod in $loaded_modules; do
		# These cannot be unloaded
		test $kmod = 'ng_ether' -o $kmod = 'ng_socket' \
			&& continue

		test_comment "Unloading $kmod..."
		kldunload $kmod
	done
	loaded_modules=''
}

configure_nodes () {
	ngctl mkpeer $eth: macfilter lower ether					# Connect the lower hook of the ether instance $eth to the ether hook of a new macfilter instance
	ngctl name $eth:lower MF							# Give the macfilter instance a name
	ngctl mkpeer $eth: one2many upper one						# Connect the upper hook of the ether instance $eth to the one hook of a new one2many instance
	ngctl name $eth:upper O2M							# Give the one2many instance a name
	ngctl msg O2M: setconfig "{ xmitAlg=3 failAlg=1 enabledLinks=[ 1 1 ] }"		# XMIT_FAILOVER -> send replies always out many0

	ngctl connect MF: O2M: default many0						# Connect macfilter:default to the many0 hook of a one2many instance
	for i in $(seq 1 1 $HOOKS); do
		ngctl connect MF: O2M: out$i many$i
	done
}

deconfigure_nodes () {
	ngctl shutdown MF:
	ngctl shutdown O2M:
}

cleanup () {
	test_title "Cleaning up"

	deconfigure_nodes
	unload_modules

	rm -f $entries_lst $entries2_lst
}

TSTNR=0
TSTFAILS=0
TSTSUCCS=0

_test_next () { TSTNR=$(($TSTNR + 1)); }
_test_succ () { TSTSUCCS=$(($TSTSUCCS + 1)); }
_test_fail () { TSTFAILS=$(($TSTFAILS + 1)); }

test_cnt () { echo "1..${1:-$TSTNR}"; }
test_title () {
	local msg="$1"

	printf '### %s ' "$msg"
	printf '#%.0s' `seq $((80 - ${#msg} - 5))`
	printf "\n"
}
test_comment () { echo "# $1"; }
test_bailout () { echo "Bail out!${1+:- $1}"; exit 1; }
test_bail_on_fail () { test $TSTFAILS -eq 0 || test_bailout $1; }
test_ok () {
	local msg="$1"

	_test_next
	_test_succ
	echo "ok $TSTNR - $msg"

	return 0
}
test_not_ok () {
	local msg="$1"

	_test_next
	_test_fails
	echo "not ok $TSTNR - $msg"

	return 1
}
test_eq () {
	local v1="$1" v2="$2" msg="$3"

	if [ "$v1" = "$v2" ]; then
		test_ok "$v1 $msg"
	else
		test_not_ok "$v1 vs $v2 $msg"
	fi
}
test_ne () {
	local v1="$1" v2="$2" msg="$3"

	if [ "$v1" != "$v2" ]; then
		test_ok "$v1 $msg"
	else
		test_not_ok "$v1 vs $v2 $msg"
	fi
}
test_lt () {
	local v1=$1 v2=$2 msg="$3"

	if [ "$v1" -lt "$v2" ]; then
		test_ok "$v1 $msg"
	else
		test_not_ok "$v1 >= $v2 $msg"
	fi
}
test_le () {
	local v1=$1 v2=$2 msg="$3"

	if [ "$v1" -le "$v2" ]; then
		test_ok "$v1 $msg"
	else
		test_not_ok "$v1 >= $v2 $msg"
	fi
}
test_gt () {
	local v1=$1 v2=$2 msg="$3"

	if [ "$v1" -gt "$v2" ]; then
		test_ok "$v1 $msg"
	else
		test_not_ok "$v1 <= $v2 $msg"
	fi
}
test_ge () {
	local v1=$1 v2=$2 msg="$3"

	if [ "$v1" -ge "$v2" ]; then
		test_ok "$v1 $msg"
	else
		test_not_ok "$v1 <= $v2 $msg"
	fi
}
test_rc ()      { test_eq $? $1 "$2"; }
test_failure () { test_ne $? 0 "$1"; }
test_success () { test_eq $? 0 "$1"; }

gethooks () {
	ngctl msg MF: 'gethooks' \
		| perl -ne '$h{$1}=1 while s/hookname="(.*?)"//; sub END {print join(":", sort keys %h)."\n"}'
}

countmacs () {
	local hookname=${1:-'[^"]*'}

	ngctl msg MF: 'gethooks' \
		| perl -ne 'sub BEGIN {$c=0} $c += $1 while s/hookname="'$hookname'" hookid=\d+ maccnt=(\d+)//; sub END {print "$c\n"}'
}
randomedge () {
	local edge="out$(seq 0 1 $HOOKS | sort -R | head -1)"
	test $edge = 'out0' \
		&& echo default \
		|| echo $edge
}
genmac () {
	echo "00:00:00:00:$(printf "%02x" $1):$(printf "%02x" $2)"
}



################################################################################
### Start ######################################################################
################################################################################

test_title "Setting up system..."
load_modules netgraph ng_socket ng_ether ng_macfilter ng_one2many
eth=$(find_iface)
test_comment "Using $eth..."


test_title "Configuring netgraph nodes..."
configure_nodes

trap 'exit 99' 1 2 3 13 14 15
trap 'cleanup' EXIT

created_hooks=$(gethooks)
rc=0

test_cnt


################################################################################
### Tests ######################################################################
################################################################################

################################################################################
test_title "Test: Duplicate default hook"
ngctl connect MF: O2M: default many99 2>/dev/null
test_failure "duplicate connect of default hook"


################################################################################
test_title "Test: Add and remove hooks"
ngctl connect MF: O2M: xxx1 many$(($HOOKS + 1))
test_success "connect MF:xxx1 to O2M:many$(($HOOKS + 1))"
ngctl connect MF: O2M: xxx2 many$(($HOOKS + 2))
test_success "connect MF:xxx2 to O2M:many$(($HOOKS + 2))"
ngctl connect MF: O2M: xxx3 many$(($HOOKS + 3))
test_success "connect MF:xxx3 to O2M:many$(($HOOKS + 3))"
hooks=$(gethooks)
test_eq $created_hooks:xxx1:xxx2:xxx3 $hooks 'hooks after adding xxx1-3'

ngctl rmhook MF: xxx1
test_success "rmhook MF:xxx$i"
hooks=$(gethooks)
test_eq $created_hooks:xxx2:xxx3 $hooks 'hooks after removing xxx1'
ngctl rmhook MF: xxx2
test_success "rmhook MF:xxx$i"
hooks=$(gethooks)
test_eq $created_hooks:xxx3 $hooks 'hooks after removing xxx2'
ngctl rmhook MF: xxx3
test_success "rmhook MF:xxx$i"
hooks=$(gethooks)
test_eq $created_hooks $hooks 'hooks after removing xxx3'

test_bail_on_fail

################################################################################
test_title "Test: Add many hooks"
added_hooks=""
for i in $(seq 10 1 $HOOKSADD); do
	added_hooks="$added_hooks:xxx$i"
	ngctl connect MF: O2M: xxx$i many$(($HOOKS + $i))
done
hooks=$(gethooks)
test_eq $created_hooks$added_hooks $hooks 'hooks after adding many hooks'

for h in $(echo $added_hooks | perl -ne 'chomp; %h=map { $_=>1 } split /:/; print "$_\n" for grep {$_} keys %h'); do
	ngctl rmhook MF: $h
done
hooks=$(gethooks)
test_eq $created_hooks $hooks 'hooks after adding many hooks'

test_bail_on_fail


################################################################################
test_title "Test: Adding many MACs..."
I=1
for i in $(seq $ITERATIONS | sort -R); do
	test_comment "Iteration $I/$iterations..."
	for j in $(seq 0 1 $SUBITERATIONS); do
		test $i = 2 && edge='out2' || edge='out1'
		ether=$(genmac $j $i)

		ngctl msg MF: 'direct' "{ hookname=\"$edge\" ether=$ether }"
	done
	I=$(($I + 1))
done

n=$(countmacs out1)
n2=$(( ( $ITERATIONS - 1 ) * ( $SUBITERATIONS + 1 ) ))
test_eq $n $n2 'MACs in table for out1'
n=$(countmacs out2)
n2=$(( 1 * ( $SUBITERATIONS + 1 ) ))
test_eq $n $n2 'MACs in table for out2'
n=$(countmacs out3)
n2=0
test_eq $n $n2 'MACs in table for out3'

test_bail_on_fail


################################################################################
test_title "Test: Changing hooks for MACs..."
for i in $(seq $ITERATIONS); do
	edge='out3'
	ether=$(genmac 0 $i)

	ngctl msg MF: 'direct' "{ hookname=\"$edge\" ether=$ether }"
done

n=$(countmacs out1)
n2=$(( ( $ITERATIONS - 1 ) * ( $SUBITERATIONS + 1 - 1 ) ))
test_eq $n $n2 'MACs in table for out1'
n=$(countmacs out2)
n2=$(( 1 * ( $SUBITERATIONS + 1 - 1 ) ))
test_eq $n $n2 'MACs in table for out2'
n=$(countmacs out3)
n2=$ITERATIONS
test_eq $n $n2 'MACs in table for out3'

test_bail_on_fail


################################################################################
test_title "Test: Removing all MACs one by one..."
I=1
for i in $(seq $ITERATIONS | sort -R); do
	test_comment "Iteration $I/$iterations..."
	for j in $(seq 0 1 $SUBITERATIONS | sort -R); do
		edge="default"
		ether=$(genmac $j $i)

		ngctl msg MF: 'direct' "{ hookname=\"$edge\" ether=$ether }"
	done
	I=$(($I + 1))
done
n=$(countmacs)
n2=0
test_eq $n $n2 'MACs in table'

test_bail_on_fail


################################################################################
test_title "Test: Randomly adding MACs on random hooks..."
rm -f $entries_lst
for i in $(seq $ITERATIONS); do
	test_comment "Iteration $i/$iterations..."
	for j in $(seq 0 1 $SUBITERATIONS | sort -R); do
		edge=$(randomedge)
		ether=$(genmac $j $i)

		ngctl msg MF: 'direct' "{ hookname=\"$edge\" ether=$ether }"

		echo $ether $edge >> $entries_lst
	done

	n=$(countmacs)
done

n=$(countmacs out1)
n2=$(grep -c ' out1' $entries_lst)
test_eq $n $n2 'MACs in table for out1'
n=$(countmacs out2)
n2=$(grep -c ' out2' $entries_lst)
test_eq $n $n2 'MACs in table for out2'
n=$(countmacs out3)
n2=$(grep -c ' out3' $entries_lst)
test_eq $n $n2 'MACs in table for out3'

test_bail_on_fail


################################################################################
test_title "Test: Randomly changing MAC assignments..."
rm -f $entries2_lst
for i in $(seq $ITERATIONS); do
	test_comment "Iteration $i/$iterations..."
	cat $entries_lst | while read ether edge; do
		edge2=$(randomedge)

		ngctl msg MF: 'direct' "{ hookname=\"$edge2\" ether=$ether }"

		echo $ether $edge2 >> $entries2_lst
	done

	n=$(countmacs out1)
	n2=$(grep -c ' out1' $entries2_lst)
	test_eq $n $n2 'MACs in table for out1'
	n=$(countmacs out2)
	n2=$(grep -c ' out2' $entries2_lst)
	test_eq $n $n2 'MACs in table for out2'
	n=$(countmacs out3)
	n2=$(grep -c ' out3' $entries2_lst)
	test_eq $n $n2 'MACs in table for out3'

	test_bail_on_fail

	mv $entries2_lst $entries_lst
done


################################################################################
test_title "Test: Resetting macfilter..."
ngctl msg MF: reset
test_success "**** reset failed"
test_eq $(countmacs) 0 'MACs in table'

test_bail_on_fail


################################################################################
test_cnt

exit 0
