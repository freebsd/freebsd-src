:
#
# ----------------------------------------------------------------------------
# "THE BEER-WARE LICENSE" (Revision 42):
# <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
# can do whatever you want with this stuff. If we meet some day, and you think
# this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
# ----------------------------------------------------------------------------
#
# $Id: kernxref.sh,v 1.1 1995/10/15 11:33:42 phk Exp $
#
# This shellscript will make a cross reference of the symbols of the LINT 
# kernel.

cd /sys/compile/LINT
nm -gon *.o /sys/libkern/obj/*.o /lkm/*.o | tr : ' ' | awk '
NF > 1	{
	if (length($2) == 8) {
		$2 = $3
		$3 = $4
	}
	if ($2 == "t") 
		next
	if ($2 == "F")
		next
	nm[$3]++
	if ($2 == "U") {
		ref[$3]=ref[$3]" "$1
	} else if ($2 == "T" || $2 == "D" || $2 == "A") {
		if (def[$3] != "")
			def[$3]=def[$3]","$1
		else
			def[$3]=$1
	} else if ($2 == "?") {
		if (def[$3] == "S")
			i++
		else if (def[$3] != "")
			def[$3]=def[$3]",S"
		else
			def[$3]="S"
		ref[$3]=ref[$3]" "$1
	} else if ($2 == "C") {
		if (def[$3] == $2)
			i++
		else if (def[$3] != "")
			def[$3]=def[$3]",C"
		else
			def[$3]="C"
		ref[$3]=ref[$3]" "$1
	} else {
		print ">>>",$0
	}
	}
END	{
	for (i in nm) {
		printf "%s {%s} %s\n",i,def[i],ref[i]
	}
	}
' | sort | awk '
	{
	if ($2 == "{S}")
		$2 = "<Linker set>"
	if ($2 == "{C}")
		$2 = "<Common symbol>"
	if (length($3) == 0) {
		printf "%-30s %s UNREF\n",$1,$2
	} else if ($2 == "{}") {
		printf "%-30s {UNDEF}\n",$1
	} else {
		printf "%-30s %s\n\t%s\n",$1,$2,$3
	}
	}
'
