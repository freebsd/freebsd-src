# parray:
# Print the contents of a global array on stdout.
#
# SCCS: @(#) parray.tcl 1.9 96/02/16 08:56:44
#
# Copyright (c) 1991-1993 The Regents of the University of California.
# Copyright (c) 1994 Sun Microsystems, Inc.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#

proc parray {a {pattern *}} {
    upvar 1 $a array
    if ![array exists array] {
	error "\"$a\" isn't an array"
    }
    set maxl 0
    foreach name [lsort [array names array $pattern]] {
	if {[string length $name] > $maxl} {
	    set maxl [string length $name]
	}
    }
    set maxl [expr {$maxl + [string length $a] + 2}]
    foreach name [lsort [array names array $pattern]] {
	set nameString [format %s(%s) $a $name]
	puts stdout [format "%-*s = %s" $maxl $nameString $array($name)]
    }
}
