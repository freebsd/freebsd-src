#!/usr/local/bin/tcl
#
# ----------------------------------------------------------------------------
# "THE BEER-WARE LICENSE" (Revision 42):
# <phk@login.dkuug.dk> wrote this file.  As long as you retain this notice you
# can do whatever you want with this stuff. If we meet some day, and you think
# this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
# ----------------------------------------------------------------------------
#
# $FreeBSD$
#
#######################################################################
# Generic procedures usable in the process of gnu-to-bmake jobs.
#

#######################################################################
# sh -- execute command.
#	argv[1] shell command to execute.
#
proc sh {cmd} {
    puts stdout "+ $cmd"
    flush stdout
    exec sh -e -c $cmd >&@ stdout
}

#######################################################################
# cp -- execute cp(1)
#	argv arguments to cp(1)
#
proc cp {args} {
    sh "cp $args"
}

#######################################################################
# copy_l -- Copy list of files, try to make(1) them if missing.
#	argv[1] source directory
#	argv[2] destination directory
#	argv[3] list of filenames
#
proc copy_l {src dst files} {
    foreach f $files {
	if {![file exists $src/${f}]} {
	    sh "cd $src ; set +e ; make ${f}"
	}
	if {![file exists $src/${f}]} {
	    error "Couldn't produce ${f} in $src"
	}
	cp $src/${f} $dst
    }
}

#######################################################################
# copy_c -- Copy list of .c files, try to make(1) them if missing.
#	argv[1] source directory
#	argv[2] destination directory
#	argv[3] list of filenames, with or without .c suffixes.
#
proc copy_c {src dst files} {
    regsub -all {\.c} $files {} files
    foreach f $files {
	if {![file exists $src/${f}.c]} {
	    sh "cd $src ; set +e ; make ${f}.c"
	}
	if {![file exists $src/${f}.c]} {
	    sh "cd $src ; set +e ; make ${f}.o"
	}
	if {![file exists $src/${f}.c]} {
	    error "Couldn't produce ${f}.c in $src"
	}
	cp $src/${f}.c $dst
    }
}

#######################################################################
# zap_suffix -- remove suffixes from list if filenames
#	argv[1] list of filenames
#	argv[2] (optional) regex matching suffixes to be removed,
#		default removes all known suffixes, (AND warts too!).
#
proc zap_suffix {lst {suf {\.[cyolhsxS]}}} {
    regsub -all $suf $lst {} a
    return $a
}

#######################################################################
# add_suffix -- add suffixes to list if filenames
#	argv[1] list of filenames
#	argv[2] string to add.
#
proc add_suffix {lst suf} {
    set l ""
    foreach i $lst {lappend l ${i}${suf}}
    return $l
}

#######################################################################
# add_prefix -- add prefixes to list if filenames
#	argv[1] list of filenames
#	argv[2] string to add.
#
proc add_prefix {lst prf} {
    set l ""
    foreach i $lst {lappend l ${prf}${i}}
    return $l
}

#######################################################################
# basename -- removes directory-prefixes from list of names.
#	argv[1] list of filenames
#
proc basename {lst} {
    set l ""
    foreach i $lst {regsub {.*/} $i {} i ; lappend l $i}
    return $l
}

#######################################################################
# makefile_macro -- return the contents of a Makefile macro
#	argv[1] name of macro
#	argv[2] source directory
#	argv[3] (optional) name of makefile
#
proc makefile_macro {macro dir {makefile Makefile}} {
    # Nobody will miss a core file, right ?
    cp $dir/$makefile $dir/make.core
    set f [open $dir/make.core a]
    puts $f "\n\nGNU2TCL_test:\n\t@echo \$\{$macro\}"
    close $f
    set a [exec make -f $dir/make.core GNU2TCL_test]
    sh "rm -f $dir/make.core"
    return $a
}

#######################################################################
# mk_prog -- Make a directory and Makefile for a program.
#	argv[1] name of the parent-directory
#	argv[2] name of the program
#	argv[3] list of .c files (the SRCS macro content).
#	argv[4] (optional) list of lines for the Makefile
#
proc mk_prog {ddir name list {make ""}} {
    sh "mkdir $ddir/$name"
    set f [open $ddir/$name/Makefile w]
    puts $f "#\n# \$FreeBSD\$\n#\n"
    puts $f "PROG =\t$name"
    puts $f "SRCS =\t[lsort $list]"
    foreach i $make {puts $f $i}
    puts $f "\n.include <bsd.prog.mk>"
    close $f
}

#######################################################################
# mk_lib -- Make a directory and Makefile for a library
#	argv[1] name of the parent-directory
#	argv[2] name of the library
#	argv[3] list of .c files (the SRCS macro content).
#	argv[4] (optional) list of lines for the Makefile
#
proc mk_lib {ddir name list {make ""}} {
    sh "mkdir $ddir/$name"
    set f [open $ddir/$name/Makefile w]
    puts $f "#\n# \$FreeBSD\$\n#\n"
    puts $f "SRCS =\t[lsort $list]"
    puts $f "LIB =\t$name"
    foreach i $make {puts $f $i}
    puts $f "\n.include <bsd.lib.mk>"
    close $f
}

#######################################################################
# common_set -- Return the files common to a list of lists.
#	argv[] lists of filenames
#
proc common_set {args} {
    set a(0) 0 ; unset a(0)
    foreach i $args {
	foreach j $i {if {[catch {incr a($j)} k]} {set a($j) 1}}
    }
    set j ""
    foreach i [array names a] {
	if {$a($i) > 1} {lappend j $i}
    }
    return $j
}

#######################################################################
# reduce_by -- Remove elements from list, if present in 2nd list.
#	argv[1] lists of filenames
#	argv[2] lists of filenames to be removed.
#
proc reduce_by {l1 l2} {
    set a(0) 0 ; unset a(0)
    foreach j $l1 { if {[catch {incr a($j)} k]} {set a($j) 1} }
    foreach j $l2 { catch {unset a($j)} }
    return [array names a]
}
