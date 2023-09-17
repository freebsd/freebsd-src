#!/bin/sh
#
# Copyright (c) 2014, 2019, 2020 Juniper Networks, Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#

#
# Convert a NIC driver to use the procdural API.
# It doesn't take care of all the # cases yet,
# but still does about 95% of work.
#
# Author: Sreekanth Rupavatharam
#

MAX_PASSES=100

if [ $# -lt 1 ]
then
	echo " $0 <driver source (e.g., if_em.c)>";
	exit 1;
fi

# XXX - This needs to change if the data structure uses different name
__ifp__="ifp";

file=$1

rotateCursor() {
	case $toggle in
	1) c="\\" ;;
	2) c="|" ;;
	3) c="/" ;;
	*) c="-" ;;
	esac
	toggle=$(((toggle + 1) % 4))
	printf " %s \b\b\b" $c
}

# Handle the case where $__ifp__->if_blah = XX;
handle_set() {
	if echo $line | grep "$__ifp__->.* = " > /dev/null 2>&1
	then
		if echo $line | grep "\[$__ifp__->.* = " > /dev/null 2>&1; then
			# Special case of array[ifp->member] = value
			return 1
		fi
		word=`echo $line | awk -F "if_" ' { print $2 }' | awk -F" =" '{ print $1 }'`
		value=`echo $line | awk -F "=" '{ print $2 }' | sed -e 's/;//g'`
		new=`echo if_set$word"\($__ifp__,"$value");"`
		new=`echo $new | sed -e 's/&/\\\&/'`
		old=`echo $line|sed -e 's/^[ 	]*//'`
		line=`echo $line| sed -e's/'$old'/'$new'/g'`
		return 0
	fi
	return 1
}

handle_inc() {
	if echo $line | grep "$__ifp__->.*++\|++$__ifp__->.*" > /dev/null 2>&1
	then
		word=`echo $line | awk -F"if_" '{ print $2 }'|awk -F"\+" '{ print $1}'`
		value=' 1';
		old=`echo $line|sed -e 's/^[ 	]*//'`
		new=`echo if_inc$word"\($__ifp__,"$value");"`
		new=`echo $new | sed -e 's/&/\\\&/'`
		line=`echo $line| sed -e's/'$old'/'$new'/g'`
		return 0;
	fi
	return 1;
}

handle_add() {
	if echo $line | grep "$__ifp__->.*+= " > /dev/null 2>&1
	then
		word=`echo $line | awk -F"if_" '{ print $2 }'|awk '{ print $1}'`
		value=`echo $line | awk -F"=" '{ print $2}' | sed -e 's/;//g'`
		new=`echo if_inc$word"\($__ifp__,$value);"`
		new=`echo $new | sed -e 's/&/\\\&/'`
		old=`echo $line|sed -e 's/^[ 	]*//'`
		line=`echo $line| sed -e's/'$old'/'$new'/g'`
		return 0
	fi
	return 1;

}

handle_or() {
	if echo $line | grep "$__ifp__->.*|= " > /dev/null 2>&1
	then
		word=`echo $line | awk -F"if_" '{ print $2 }'|awk '{ print $1}'`
		value=`echo $line | awk -F"=" '{ print $2}' | sed -e 's/;//g'`
		new=`echo if_set${word}bit"($__ifp__,$value, 0);"`
		new=`echo $new | sed -e 's/&/\\\&/'`
		#line=`echo $line|sed -e 's/&/\\&/'`
		old=`echo $line|sed -e 's/^[ 	]*//'`
		line=`echo $line| sed -e's/'$old'/'$new'/g'`
		return 0;
	fi
	return 1;

}

handle_and() {
	if echo $line |grep "$__ifp__->.*&= " > /dev/null 2>&1
	then
		word=`echo $line | awk -F"if_" '{ print $2 }'|awk '{ print $1}'`
		value=`echo $line | awk -F"=" '{ print $2}' | sed -e 's/;//g'`
		value=`echo $value | sed -e's/~//g'`
		new=`echo if_set${word}bit"\($__ifp__, 0,$value);"`
		new=`echo $new | sed -e 's/&/\\\&/'`
		old=`echo $line|sed -e 's/^[ 	]*//'`
		line=`echo $line| sed -e's/'$old'/'$new'/g'`
		return 0;
	fi
	return 1;

}

handle_toggle() {
	if echo $line | grep "\^=" > /dev/null 2>&1
	then
		line=`echo $line | sed -e 's/'"$__ifp__"'->if_\(.*\) ^=\(.*\);/if_toggle\1('"$__ifp__"',\2);/g'`
		return 0;
	fi
	return 1

}

# XXX - this needs updating
handle_misc() {
	if echo $line | grep "$__ifp__->\(if_capabilities\|if_flags\|if_softc\|if_capenable\|if_hwassist\|if_mtu\|if_drv_flags\|if_index\|if_alloctype\|if_dname\|if_xname\|if_addr\|if_hw_tsomax\|if_hw_tsomaxsegcount\|if_hw_tsomaxsegsize\)" > /dev/null 2>&1
	then
		word=`echo $line |awk -F"$__ifp__->if_" '{ print $2 }' | \
			sed -e's/[^a-zA-Z0-9_]/\@/'|awk -F"\@" '{ print $1}'`
		old=`echo "$__ifp__->if_"${word}`
		new=`echo "if_get"${word}"($__ifp__)"`
		new=`echo $new | sed -e 's/&/\\\&/'`
		line=`echo $line| sed -e's/'$old'/'$new'/g'`
		return 0;
	fi
	return 1;

}

replace_str ()
{
	orig=$1
	new=$2

	if echo $line | grep "$orig" > /dev/null 2>&1
	then
		line=`echo $line | sed -e "s|$orig|$new|"`
	else
		return 1
	fi
}

handle_special ()
{
	replace_str "(\*$__ifp__->if_input)" "if_input" || \
	replace_str "IFQ_DRV_IS_EMPTY(&$__ifp__->if_snd)" \
		"if_sendq_empty($__ifp__)" || \
	replace_str "IFQ_DRV_PREPEND(&$__ifp__->if_snd" \
		"if_sendq_prepend($__ifp__" || \
	replace_str "IFQ_SET_READY(&$__ifp__->if_snd)" \
		"if_setsendqready($__ifp__)" || \
	replace_str "VLAN_CAPABILITIES($__ifp__)" \
		"if_vlancap($__ifp__)" || \
	replace_str "IFQ_SET_MAXLEN(&$__ifp__->if_snd," \
		"if_setsendqlen($__ifp__," || \
	replace_str "IFQ_DRV_DEQUEUE(&$__ifp__->if_snd, \(.*\))" \
		"\1 = if_dequeue($__ifp__)"
	replace_str "$__ifp__->if_vlantrunk != NULL" \
		"if_vlantrunkinuse($__ifp__)"
}

handle_ifps() {
	handle_set || handle_inc || handle_add || handle_or || handle_and || \
	handle_toggle || handle_misc || handle_special
}

handle_renames ()
{
	replace_str "if_setinit(" "if_setinitfn(" || \
	replace_str "if_setioctl(" "if_setioctlfn(" || \
	replace_str "if_setqflush(" "if_setqflushfn(" || \
	replace_str "if_settransmit(" "if_settransmitfn(" || \
	replace_str "if_getdrv_flags(" "if_getdrvflags(" || \
	replace_str "if_setdrv_flagsbit(" "if_setdrvflagbits(" || \
	replace_str "if_setstart(" "if_setstartfn(" || \
	replace_str "if_sethwassistbit(" "if_sethwassistbits(" || \
	replace_str "ifmedia_init(" "ifmedia_init_drv("
}

check_ifp()
{
	case "$line" in
	*"${__ifp__}->"*) return 0;; # Still an ifp to convert
	esac
	return 1
}

add_failed ()
{
	line="$line /* ${FAIL_PAT} */"
	return 1
}

if [ -e $file.tmp ]
then
	rm $file.tmp
fi
IFS=
echo -n "Conversion for $file started, please wait: "
FAIL_PAT="XXX - IFAPI"
count=0
while read -r line
do
	rotateCursor

	# There is an ifp, we need to process it
	passes=0
	while check_ifp
	do
		if handle_ifps
		then
			handle_renames
		else
			add_failed
			break
		fi
		passes=$((passes + 1))
		if [ $passes -ge $MAX_PASSES ]; then
			add_failed
			break
		fi
	done

	# Replace the ifnet * with if_t
	case "$line" in
	*"struct ifnet"*)
		line=`echo $line | sed -e 's/struct ifnet[ \t]*\*/if_t /g'` ;;
	*"IF_LLADDR("*)
		line=`echo $line | sed -e 's/IF_LLADDR(/if_getlladdr(/g'` ;;
	esac
	printf "%s\n" "$line" >> $file.tmp
done < $1
echo ""
count=`grep $FAIL_PAT $file.tmp | wc -l`
if [ $count -gt 0 ]
then
	echo "$count lines could not be converted to IFAPI"
	echo "Look for /* $FAIL_PAT */ in the converted file"
fi
echo "original $file  has been moved to $file.orig"
mv $file $file.orig
mv $file.tmp $file
