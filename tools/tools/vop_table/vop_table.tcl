#!/bin/sh

proc do_file {file} {
	global names ops op
	set f [open $file]

	set s 0

	while {[gets $f a] >= 0} {
		if {$s == 0} {
			if {[regexp {struct[ 	]*vnodeopv_entry_desc} "$a"]} {
				regsub {.*vnodeopv_entry_desc[ 	]*} $a {} a
				regsub {\[.*} $a {} a
				regsub {_entries} $a {} a
				set name $a
				set names($a) 0
				set s 1
			}
			continue
		}
		if {$s == 1} {
			if {[regexp {NULL} "$a"]} {
				set s 0
				continue
			}
			if {![regexp {vop.*_desc} "$a"]} continue
			regsub -all {[,&]} $a " " a
			set b [lindex $a 0]
			#puts "$name>> [lindex $b 0] >> [lindex $b 3]"
			set o [lindex $b 0]
			regsub {_desc} $o "" o
			set ops($o) 0
			set op([list $name $o]) [lindex $b 3]
			continue
		}
		puts "$s>> $a"
	}
	close $f
}

set fi [open "|find [glob /usr/src/sys/*fs /usr/src/sys/gnu/*fs] /usr/src/sys/kern -type f -name *.c -print | xargs grep VNODEOP_SET" ]
while {[gets $fi a] >= 0} {
	puts stderr $a
	if {[regexp {#define} $a]} continue
	if {[regexp {mallocfs} $a]} continue
	do_file [lindex [split $a :] 0]
}
close $fi

puts {<HTML>
<HEAD></HEAD><BODY>
<TABLE BORDER WIDTH="100%" NOSAVE>
}

set opn [lsort [array names ops]]
set a [lsort [array names names]]

set tbn default_vnodeop
foreach i $a {
	if {$i == "default_vnodeop"} continue
	lappend tbn $i
}

foreach i $opn {
	if {$i == "vop_default"} continue
	regsub "vop_" $i "" i
        lappend fl [format "%12s" $i]
}

lappend fl [format "%12s" default]

puts {<TR>}
puts {<TD>}
puts {</TD>}
puts "<TR>"
        puts "<TD></TD>"
	foreach j $fl {
		puts "<TD>"

		for {set i 0} {$i < 12} {incr i} {
			puts "[string index $j $i]<BR>"
		}
		puts "</TD>"
	}
puts "</TR>"

set fnames(vop_defaultop) *
set fnames(vop_nolock) nl
set fnames(vop_noislocked) ni
set fnames(vop_nounlock) nu
set fnames(vop_stdlock) sl
set fnames(vop_stdislocked) si
set fnames(vop_stdunlock) su
set fnames(vop_einval) I
set fnames(vop_enotty) T
set fnames(vop_null) -
set fnames(vop_eopnotsupp) S
set fnames(ufs_missingop) M
set fnames(vop_nopoll) np
set fnames(vop_nostrategy) ns
set fnames(vop_revoke) vr
set fnames(vfs_cache_lookup) cl
set fnames(vop_stdpathconf) pc
set fnames(vop_stdbwrite) bw

set fn 0
set nop(aa) 0
unset nop(aa)
foreach i $tbn {
	puts {<TR>}
	puts "<TD>$i</TD>"
	foreach j $opn {
		if {$j == "vop_default"} continue
		if {![info exists op([list $i $j])]} {
			puts "<TD></TD>"
			continue
		}
		set t $op([list $i $j])
	
		set c "#00ddd0"
		if {[info exists fnames($t)]} {
			set t $fnames($t)
		} else {
			if {![info exists nop($t)]} {
				incr fn
				set nop($t) $fn
				set nfn($fn) $t
				set use($fn) 0
			}
			incr use($nop($t))
			set t "<FONT SIZE=-1>$nop($t)</FONT>"
			set c "#00ffd0"
		}
		puts "<TD BGCOLOR=\"$c\">$t</TD>"
	}
	set j vop_default
	if {![info exists op([list $i $j])]} {
		puts "<TD></TD>"
		continue
	}
	puts "<TD>$op([list $i $j])</TD>"

	puts "</TR>"
}
puts "</TABLE>"
puts "<HR>"
puts {<PRE>}
foreach i [lsort [array names fnames]] {
	puts [format "%-2s %s" $fnames($i)  $i]
}
puts {
</PRE>
}
puts "<HR>"
puts "<HR>"
puts {<TABLE BORDER NOSAVE>}
set m 8
for {set i 1} {$i <= $fn} {incr i $m} {
	puts "<TR>"
	for {set j 0} {$j < $m} {incr j} {
		set k [expr $i + $j]
		if {$k <= $fn} {
			#puts "<TD>$k</TD><TD><FONT SIZE=-1>$nfn($k)/$use($k)</FONT></TD>"
			puts "<TD>$k</TD><TD><FONT SIZE=-1>$nfn($k)</FONT></TD>"
		}
	}
	puts "</TR>"
}
puts "</TABLE>"

puts "</TABLE>"
puts "</BODY>"
puts "</HTML>"
